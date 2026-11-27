#pragma once
#include <QString>

#define SETTINGS_LIST	\
	X(repeatCount,	int,	 10)\
	X(baseSeed,		int,	 -1)\
	X(seedDelta,	int,	 1)\
	X(width,		int,	 1024)\
	X(height,		int,	 1024)\
	X(steps,		int,	 20)\
	X(cfgScale,		double,  3.5)\
	X(outPath,		QString, "./output")\
	X(serverUrl,    QString, "http://127.0.0.1:1234/sdapi/v1/txt2img")

struct Settings
{
	void loadSettings();
	void saveSettings();

#define X(var, type, def)  type var = def;
	SETTINGS_LIST
#undef X
};

