package com.thelogicmaster.switchgdx;

import arc.Files;
import arc.backend.switchgdx.SwitchApplication;
import arc.util.Log;
import mindustry.ClientLauncher;
import mindustry.Vars;
import mindustry.core.Version;

public class SwitchLauncher extends ClientLauncher {

	public static void main (String[] arg) {
		Log.level = Log.LogLevel.debug;
		Vars.loadLogger();
		new SwitchApplication(new SwitchLauncher(arg), new SwitchApplication.Config());
	}

	public SwitchLauncher(String[] args) {
//		Version.init();
	}
}
