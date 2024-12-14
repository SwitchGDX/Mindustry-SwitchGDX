package com.thelogicmaster.switchgdx;

import arc.backend.switchgdx.SwitchApplication;
import arc.files.Fi;
import arc.util.Log;
import mindustry.ClientLauncher;
import mindustry.Vars;

public class SwitchLauncher extends ClientLauncher {

	@Override
	public ClassLoader loadJar(Fi jar, ClassLoader parent) throws Exception {
		return parent;
	}

	public static void main (String[] arg) {
		Log.level = Log.LogLevel.debug;
		Vars.loadLogger();
		new SwitchApplication(new SwitchLauncher(), new SwitchApplication.Config());
	}
}
