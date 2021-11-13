#include "scene-notes-dock.hpp"
#include <obs-module.h>
#include <QMainWindow>
#include <QVBoxLayout>

#include "version.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("scene-notes-dock", "en-US")

bool obs_module_load()
{
	blog(LOG_INFO, "[Scene Notes Dock] loaded version %s", PROJECT_VERSION);

	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_push_ui_translation(obs_module_get_string);
	obs_frontend_add_dock(new SceneNotesDock(main_window));
	obs_frontend_pop_ui_translation();

	return true;
}

void obs_module_unload() {}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("SceneNotesDock");
}

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

static void frontend_event(enum obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
		auto *scene = obs_frontend_get_current_scene();
		if (!scene)
			return;

		auto *settings = obs_source_get_settings(scene);
		if (settings) {
			auto *dock = static_cast<SceneNotesDock *>(data);
			dock->textEdit->setHtml(QT_UTF8(obs_data_get_string(settings, "notes")));
			obs_data_release(settings);
		}
		obs_source_release(scene);
	}
}

SceneNotesDock::SceneNotesDock(QWidget *parent) : QDockWidget(parent)
{
	setFeatures(DockWidgetMovable | DockWidgetFloatable);
	setWindowTitle(QT_UTF8(obs_module_text("SceneNotes")));
	setObjectName("SceneNotesDock");
	setFloating(true);
	hide();

	auto *mainLayout = new QVBoxLayout(this);

	textEdit = new QTextEdit(this);

	mainLayout->addWidget(textEdit);

	auto *dockWidgetContents = new QWidget;
	dockWidgetContents->setLayout(mainLayout);
	setWidget(dockWidgetContents);

	auto changeText = [this]() {
		auto *scene = obs_frontend_get_current_scene();
		if (!scene)
			return;
		auto *settings = obs_source_get_settings(scene);
		if (settings) {
			auto old_notes =
				QT_UTF8(obs_data_get_string(settings, "notes"));
			auto new_notes = textEdit->toHtml();
			if (old_notes != new_notes) {
				obs_data_set_string(settings, "notes",
						    QT_TO_UTF8(new_notes));
			}
			obs_data_release(settings);
		}
		obs_source_release(scene);
	};
	connect(textEdit, &QTextEdit::textChanged, changeText);

	obs_frontend_add_event_callback(frontend_event, this);
}

SceneNotesDock::~SceneNotesDock()
{
	obs_frontend_remove_event_callback(frontend_event, this);
}
