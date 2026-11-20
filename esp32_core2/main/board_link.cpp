#include "board_link.h"
#include "certabo_session.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <NimBLEDevice.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>
namespace core2 {
namespace {
std::mutex lock;
LinkView view;
std::atomic<bool> rescan{false}, startingPosition{false};
std::atomic<int> selected{-1};
std::atomic<uint64_t> lights{0};
std::atomic<int> ledEval{0};
std::atomic<uint32_t> rxSequence{0};
uint32_t expectedSequence = 0;
CertaboSession certaboSession;
NimBLEClient *client = nullptr;
NimBLERemoteCharacteristic *writer = nullptr;
NimBLERemoteCharacteristic *positionWriter = nullptr;
bool isNut = false, isCertabo = false;
uint32_t lastWriteAt = 0;
int failedWrites = 0;
struct Packet {
  uint32_t sequence;
  uint16_t len;
  uint8_t data[256];
};
QueueHandle_t packets;
uint32_t now() { return uint32_t(esp_timer_get_time() / 1000); }
void notify(NimBLERemoteCharacteristic *, uint8_t *data, size_t len, bool) {
  if (!packets)
    return;
  for (size_t offset = 0; offset < len; offset += sizeof(Packet::data)) {
    Packet p{};
    p.sequence = rxSequence.fetch_add(1);
    p.len = std::min(len-offset, sizeof(p.data));
    std::memcpy(p.data, data+offset, p.len);
    // A later sequence gap invalidates partial frames and learning windows.
    xQueueSend(packets, &p, 0);
  }
}
void publish(const Board &b) {
  std::lock_guard<std::mutex> g(lock);
  if (!view.haveBoard || view.board != b) {
    view.board = b;
    printf("CORE2 BOARD position received\n");
    ++view.revision;
  }
  view.haveBoard = true;
}
bool write(const std::vector<uint8_t> &bytes) {
  if (!client || !client->isConnected() || !writer)
    return false;
  uint32_t gap = now() - lastWriteAt;
  if (!isCertabo && lastWriteAt && gap < 100)
    vTaskDelay(pdMS_TO_TICKS(100 - gap));
  lastWriteAt = now();
  const size_t mtu =
      std::max<size_t>(20, client->getMTU() > 3 ? client->getMTU() - 3 : 20);
  for (size_t off = 0; off < bytes.size(); off += mtu) {
    size_t n = std::min(mtu, bytes.size() - off);
    if (!writer->writeValue(bytes.data() + off, n, !isCertabo)) {
      if (isCertabo || ++failedWrites >= 3)
        client->disconnect();
      return false;
    }
    failedWrites = 0;
    if (off + n < bytes.size())
      vTaskDelay(pdMS_TO_TICKS(5));
  }
  return true;
}
NimBLERemoteCharacteristic *find(const char *uuid) {
  for (auto *service : client->getServices()) {
    auto *c = service->getCharacteristic(uuid);
    if (c)
      return c;
  }
  return nullptr;
}
bool connect(const Peer &peer) {
  if (!client) {
    client = NimBLEDevice::createClient();
    client->setConnectTimeout(8000);
    client->setConnectionParams(12, 24, 0, 200);
  }
  if (client->isConnected())
    client->disconnect();
  writer = positionWriter = nullptr;
  startingPosition = false;
  isNut = peer.chessnut;
  isCertabo = peer.certabo;
  certaboSession.reset();
  xQueueReset(packets);
  rxSequence = 0; expectedSequence = 0;
  lastWriteAt = 0; failedWrites = 0;
  {
    std::lock_guard<std::mutex> g(lock);
    view.connecting = true;
    view.connected = false;
    view.haveBoard = false;
    view.virtualPosition = false;
    view.certabo = isCertabo;
    view.spectrum = false;
    view.calibrating = isCertabo;
    view.name = peer.name;
    view.status = "Verbinde Brett";
  }
  printf("CORE2 CONNECT begin %s %s type=%u nut=%d\n", peer.name.c_str(), peer.address.c_str(), peer.type, peer.chessnut);
  const uint16_t previousMtu = NimBLEDevice::getMTU();
  if (isCertabo) NimBLEDevice::setMTU(256);
  const bool radioConnected = client->connect(NimBLEAddress(peer.address, peer.type));
  if (isCertabo) NimBLEDevice::setMTU(previousMtu);
  if (!radioConnected) {
    printf("CORE2 CONNECT radio failed error=%d\n", client->getLastError());
    return false;
  }
  // getServices() alone only returns the cached vector in NimBLE 2.x.
  // Explicit discovery must precede searching across services for Chessnut.
  const auto &services = client->getServices(true);
  printf("CORE2 CONNECT discovered services=%u\n", unsigned(services.size()));
  if (services.empty()) {
    client->disconnect();
    return false;
  }
  NimBLERemoteCharacteristic *reader = nullptr;
  if (isNut) {
    writer = find("1b7e8272-2877-41c3-b46e-cf057c562023");
    reader = find("1b7e8262-2877-41c3-b46e-cf057c562023");
  } else if (isCertabo) {
    auto *s = client->getService("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
    if (s) {
      writer = s->getCharacteristic("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
      reader = s->getCharacteristic("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
    }
  } else {
    auto *s = client->getService("49535343-fe7d-4ae5-8fa9-9fafd205e455");
    if (s) {
      positionWriter = s->getCharacteristic("773a1001-7e5b-4c2a-9d13-619b8e04f260");
      writer = s->getCharacteristic("49535343-8841-43f4-a8d4-ecbe34729bb3");
      reader = s->getCharacteristic("49535343-1e4d-4bd9-ba61-23c647249616");
    }
  }
  printf("CORE2 CONNECT characteristics writer=%d reader=%d notify=%d\n", writer != nullptr, reader != nullptr, reader && reader->canNotify());
  if (!writer || (isCertabo && !writer->canWriteNoResponse()) ||
      !reader || !reader->canNotify() ||
      !reader->subscribe(true, notify)) {
    client->disconnect();
    return false;
  }
  if (isNut) {
    auto *misc = find("1b7e8273-2877-41c3-b46e-cf057c562023");
    if (misc && misc->canNotify())
      misc->subscribe(true, [](NimBLERemoteCharacteristic *, uint8_t *, size_t, bool) {});
  }
  bool ok = isCertabo || (isNut ? write({0x21, 0x01, 0x00}) : write(modeCommand("S")));
  if (!ok) {
    client->disconnect();
    return false;
  }
  {
    std::lock_guard<std::mutex> g(lock);
    view.connecting = false;
    view.connected = true;
    view.virtualPosition = positionWriter && positionWriter->canWrite();
    view.status = isCertabo ? certaboSession.status() : "Brett verbunden";
  }
  printf("CORE2 BOARD connected %s %s\n", peer.name.c_str(),
         peer.address.c_str());
  return true;
}
void task(void *) {
  try {
    NimBLEDevice::init("Virgin Core2");
    NimBLEDevice::setMTU(247);
    NimBLEDevice::setPower(6);
    auto *scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(120);
    scan->setWindow(80);
    uint32_t lastPoll = 0, connectedAt = 0;
    uint64_t sent = ~uint64_t(0);
    std::vector<uint8_t> stream, sentCertabo;
    uint32_t certaboSentAt = 0;
    while (true) {
      if (rescan.exchange(false)) {
        if (client && client->isConnected()) {
          if (isCertabo) {
            // Clear only after the queen gesture has selected the LED format.
            auto off = certaboSession.clearLeds();
            if (!off.empty()) write(off);
          } else write(isNut ? chessnutLeds(0) : modeLeds(0));
          client->disconnect();
        }
        writer = nullptr;
        {
          std::lock_guard<std::mutex> g(lock);
          view.scanning = true;
          view.connecting = false;
          view.connected = false;
          view.haveBoard = false;
          view.certabo = view.spectrum = view.calibrating = false;
          view.count = 0;
          view.status = "Suche E-Boards";
        }
        auto found = scan->getResults(4000);
        std::vector<Peer> matches;
        for (int i = 0; i < found.getCount(); i++) {
          auto *d = found.getDevice(i);
          auto name = d->getName();
          printf("CORE2 SCAN name=%s address=%s rssi=%d supported=%d\n", name.c_str(), d->getAddress().toString().c_str(), d->getRSSI(), supportedName(name));
          if (!supportedName(name))
            continue;
          bool duplicate = false;
          std::string address = d->getAddress().toString();
          for (const auto &p : matches)
            duplicate |= p.address == address;
          if (!duplicate)
            matches.push_back({name, address, d->getAddress().getType(),
                               d->getRSSI(), chessnutName(name), certaboName(name)});
        }
        std::sort(matches.begin(), matches.end(),
                  [](const Peer &a, const Peer &b) { return a.rssi > b.rssi; });
        {
          std::lock_guard<std::mutex> g(lock);
          view.count = std::min<size_t>(5, matches.size());
          for (unsigned i = 0; i < view.count; i++) {
            view.peers[i] = matches[i];
          }
          view.scanning = false;
          view.status =
              matches.empty() ? "Kein Brett gefunden" : "Brett antippen";
        }
        scan->clearResults();
      }
      int choose = selected.exchange(-1);
      if (choose >= 0) {
        Peer peer;
        bool valid = false;
        {
          std::lock_guard<std::mutex> g(lock);
          if (unsigned(choose) < view.count) {
            peer = view.peers[choose];
            valid = true;
          }
        }
        if (valid) {
          bool ok = connect(peer);
          stream.clear();
          sentCertabo.clear(); certaboSentAt = 0;
          sent = ~uint64_t(0);
          connectedAt = now();
          lastPoll = 0;
          if (!ok) {
            {
              std::lock_guard<std::mutex> g(lock);
              view.connecting = false;
              view.connected = false;
              view.status = "Verbindung fehlgeschlagen";
            }
          }
        }
      }
      if (client && client->isConnected() && writer) {
        if (startingPosition.exchange(false) && positionWriter) {
          // The payload is the complete starting board, not a reset opcode.
          static constexpr char initial[] = "RNBQKBNRPPPPPPPP................................pppppppprnbqkbnr";
          Board board{}; std::copy_n(initial,64,board.begin());
          auto bytes = modePosition(board);
          xQueueReset(packets); stream.clear(); expectedSequence = rxSequence.load();
          { std::lock_guard<std::mutex> g(lock); view.haveBoard = false; }
          const bool ok = positionWriter->writeValue(bytes.data(),bytes.size(),true);
          printf("CORE2 START_POSITION sent=%d connected=%d\n",ok,client->isConnected());
          sent = ~uint64_t(0); lastPoll = 0;
          if (!ok) { std::lock_guard<std::mutex> g(lock); view.status = "Grundstellung nicht gesendet"; }
        }
        Packet p{};
        while (xQueueReceive(packets, &p, 0) == pdTRUE) {
          if (p.sequence != expectedSequence) {
            stream.clear(); certaboSession.gap(); sentCertabo.clear();
            printf("CORE2 RX_GAP expected=%u received=%u\n", unsigned(expectedSequence), unsigned(p.sequence));
          }
          expectedSequence = p.sequence + 1;
          if (isCertabo) {
            for (unsigned i=0;i<p.len;++i) {
              Board board{};
              if (certaboSession.feed(p.data[i],board)) publish(board);
            }
            std::lock_guard<std::mutex> g(lock);
            view.spectrum = certaboSession.spectrum();
            view.calibrating = !certaboSession.ready();
            view.status = certaboSession.status();
            continue;
          }
          stream.insert(stream.end(), p.data, p.data + p.len);
          while (!stream.empty()) {
            size_t expected = 0;
            if (isNut) {
              if (stream.size() < 2)
                break;
              if (stream[0] == 1 && stream[1] == 0x24)
                expected = 34;
            } else {
              switch (stream[0] & 127) {
              case 's':
                expected = 67;
                break;
              case 'x':
              case 'l':
                expected = 3;
                break;
              case 'r':
              case 'v':
              case 'w':
                expected = 7;
                break;
              }
            }
            if (!expected) {
              stream.erase(stream.begin());
              continue;
            }
            if (stream.size() < expected)
              break;
            Board b{};
            bool valid = isNut ? chessnutFrame(stream.data(), expected, b)
                               : modeFrame(stream.data(), expected, b);
            if (valid)
              publish(b);
            stream.erase(stream.begin(), stream.begin() + expected);
          }
          if (stream.size() > 512)
            stream.clear();
        }
        bool have = false;
        {
          std::lock_guard<std::mutex> g(lock);
          have = view.haveBoard;
        }
        if (!isCertabo && now() - lastPoll > 500) {
          if (!isNut)
            write(modeCommand("S"));
          else if (!have)
            write({0x21, 0x01, 0x00});
          lastPoll = now();
        }
        uint64_t wanted = lights.load();
        if (isCertabo) {
          auto command = certaboSession.leds(wanted,ledEval.load(),now());
          const uint32_t interval = certaboSession.spectrum() ? 200 : 600;
          if (!command.empty() && (sentCertabo.empty() || now()-certaboSentAt >= interval) &&
              (command != sentCertabo || now()-certaboSentAt >= 10000)) {
            if (write(command)) {
              sentCertabo = command; certaboSentAt = now();
              certaboSession.sent(certaboSentAt);
            }
          }
        } else if (wanted != sent) {
          if (write(isNut ? chessnutLeds(wanted) : modeLeds(wanted)))
            sent = wanted;
        }
        if (!isCertabo && !have && now() - connectedAt > 10000) {
          client->disconnect();
        }
      } else {
        bool was;
        {
          std::lock_guard<std::mutex> g(lock);
          was = view.connected;
          view.connected = false;
          if (was) {
            view.haveBoard = false;
            view.virtualPosition = false;
            view.calibrating = false;
            view.status = "Verbindung getrennt";
          }
        }
        if (was) {
          writer = nullptr;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(30));
    }
  } catch (...) {
    std::lock_guard<std::mutex> g(lock);
    view.status = "Bluetooth konnte nicht starten";
    view.scanning = false;
    view.connecting = false;
  }
  vTaskDelete(nullptr);
}
} // namespace
void linkStart() {
  packets = xQueueCreate(32, sizeof(Packet));
  if (!packets || xTaskCreatePinnedToCore(task, "board_link", 8192, nullptr, 3,
                                          nullptr, 0) != pdPASS) {
    std::lock_guard<std::mutex> g(lock);
    view.status = "Bluetooth-Speicher fehlt";
  }
}
LinkView linkView() {
  std::lock_guard<std::mutex> g(lock);
  return view;
}
bool linkStartingPosition() {
  std::lock_guard<std::mutex> g(lock);
  if (!view.connected || !view.virtualPosition) return false;
  startingPosition = true;
  return true;
}
void linkScan() { rescan = true; }
void linkChoose(unsigned i) { selected = int(i); }
void linkLeds(uint64_t m, int playerEval) { lights = m; ledEval = playerEval; }
} // namespace core2
