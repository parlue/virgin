package de.dirksommerfeld.virgin.oex;

import android.app.Activity;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.*;

/** Installation/help screen; the chess GUI controls all UCI options. */
public final class MainActivity extends Activity {
 @Override public void onCreate(Bundle state) {
  super.onCreate(state);
  final int pad=(int)(24*getResources().getDisplayMetrics().density);
  LinearLayout box=new LinearLayout(this);box.setOrientation(LinearLayout.VERTICAL);
  box.setGravity(Gravity.CENTER_HORIZONTAL);box.setPadding(pad,pad,pad,pad);box.setBackgroundColor(0xff10243b);
  box.setOnApplyWindowInsetsListener((v,insets)->{v.setPadding(pad,pad+insets.getSystemWindowInsetTop(),pad,pad+insets.getSystemWindowInsetBottom());return insets;});
  ImageView icon=new ImageView(this);icon.setImageResource(R.drawable.engine_icon);
  box.addView(icon,new LinearLayout.LayoutParams(pad*5,pad*5));
  TextView title=new TextView(this);title.setText("Virgin 0.8.5");title.setTextColor(0xffebcb82);title.setTextSize(28);box.addView(title);
  TextView text=new TextView(this);text.setTextColor(0xffe7eff8);text.setTextSize(17);text.setPadding(0,pad,0,pad);
  text.setText("Schachengine von Dirk D. Sommerfeld\n\nDie OEX-Engine ist installiert. Öffne jetzt eine Schach-App mit OEX-Unterstützung und wähle dort Virgin 0.8.5 aus.\n\nAdaptive, Spielstil und Elo-Stärke stellst du in den UCI-Optionen deiner Schach-App ein.\n\nKein Netzdownload und keine zusätzlichen Engine-Dateien erforderlich.");
  box.addView(text,new LinearLayout.LayoutParams(-1,-2));
  Button done=new Button(this);done.setText("Schließen");done.setOnClickListener(v->finish());box.addView(done);
  ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.addView(box);setContentView(scroll);
 }
}
