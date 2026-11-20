"""Check global OwnBook, profile independence, limits, and book-free adaptation."""
import collections
import sys
import chess
from virgin_adaptive_uci_test import Engine

engine = Engine(sys.argv[1])

def search(position, extra=''):
    engine.sync('position ' + position)
    lines = engine.search('go depth 3 ' + extra)
    return lines[-1].split()[1], '\n'.join(lines)

try:
    engine.send('uci')
    assert 'option name OwnBook type check default true' in engine.until('uciok')
    profiles = [f'v{elo}-{style}' for elo in range(700, 2801, 50)
                for style in ['fortress', 'stone', 'normal', 'fire', 'thunder']]
    for profile in profiles:
        engine.sync('setoption name Bot value ' + profile, 'ucinewgame')
        for start in ['startpos', 'startpos moves e2e4']:
            move, log = search(start)
            assert 'info string book selected ' in log, (profile, log)
            assert int(log.split('data_bytes ')[1].split()[0]) < 10000
            board = chess.Board()
            if start.endswith('e2e4'): board.push_uci('e2e4')
            assert chess.Move.from_uci(move) in board.legal_moves
    print('PASS global book: all 215 bot profiles, both colours', flush=True)
    engine.sync('setoption name UCI_LimitStrength value false', 'setoption name Adaptive value true')
    replies = collections.Counter()
    for _ in range(64):
        engine.sync('ucinewgame')
        move, log = search('startpos moves e2e4')
        assert 'info string book selected ' in log
        again, _ = search('startpos moves e2e4')
        assert move == again
        replies[move] += 1
    assert len(replies) > 1, replies
    print('PASS adaptive book variety and stable re-analysis:', dict(replies), flush=True)
    for position in ['startpos moves e2e4 a7a6',
                     'startpos moves e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7']:
        _, log = search(position)
        assert 'info string book selected ' not in log, log
    move, log = search('startpos moves e2e4', 'searchmoves a7a6')
    assert move == 'a7a6' and 'info string book selected ' not in log
    engine.sync('position startpos')
    engine.send('go infinite')
    active = engine.until('info depth 2 ')
    engine.send('stop')
    active += engine.until('bestmove ')
    assert not any('info string book selected ' in line for line in active)
    for adaptive in [True, False]:
        engine.sync('setoption name Adaptive value ' + str(adaptive).lower(),
                    'setoption name OwnBook value false', 'ucinewgame')
        _, log = search('startpos')
        assert 'info string book selected ' not in log and 'info depth 3 ' in log, log
    # Setting OwnBook during adaptive mode must survive restoring the old profile.
    engine.sync('setoption name OwnBook value true', 'setoption name Adaptive value true',
                'setoption name OwnBook value false', 'setoption name Adaptive value false')
    _, log = search('startpos')
    assert 'info string book selected ' not in log
    engine.sync('setoption name OwnBook value true', 'ucinewgame')
    _, log = search('startpos')
    assert 'info string book selected ' in log
    print('PASS book exit, ten-ply limit, searchmoves, analysis, global on/off and persistence', flush=True)
    # With book off, exact-score quiet alternatives can vary even in depth searches.
    engine.sync('setoption name Adaptive value true', 'setoption name OwnBook value false')
    choices = set()
    for _ in range(32):
        engine.sync('ucinewgame')
        move, log = search('startpos moves d2d4', 'searchmoves d7d5 g8f6')
        assert 'info string book selected ' not in log
        assert 'info string adaptive ' in log
        choices.add(move)
    assert choices == {'d7d5', 'g8f6'}, choices
    print('PASS adaptive randomisation without book or timed-search requirement', flush=True)
finally:
    engine.close()
