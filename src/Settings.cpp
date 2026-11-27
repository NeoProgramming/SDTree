#include "Settings.h"
#include <QSettings>

static const char INI_FILE[] = "sdtree.ini";



// Qt does not QVariant.getValue() method
template<typename T>
void getValue(const QVariant &v, T &x)
{
	x = v.value<T>();
}

void Settings::loadSettings()
{
	QSettings settings(INI_FILE, QSettings::IniFormat);
	
#define X(var, type, def)	getValue(settings.value(#var, def), var);
	SETTINGS_LIST
#undef X

	if (settings.allKeys().size() != 6)
		saveSettings();
}

void Settings::saveSettings()
{
	QSettings settings(INI_FILE, QSettings::IniFormat);

#define X(var, type, def)	settings.setValue(#var, var);
	SETTINGS_LIST
#undef X
}

