#include "scene-notes-dock.hpp"
#include <obs-module.h>
#include <QColorDialog>
#include <QFileDialog>
#include <QFontDialog>
#include <QMainWindow>
#include <QMenu>
#include <QStyle>
#include <QVBoxLayout>
#include <QTextList>
#include <QScrollBar>
#include <QSpinBox>
#include <QSlider>
#include <QWidgetAction>
#include <QDesktopServices>

#include "version.h"
#include "util/config-file.h"
#include "util/platform.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("scene-notes-dock", "en-US")

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

bool obs_module_load()
{
	blog(LOG_INFO, "[Scene Notes Dock] loaded version %s", PROJECT_VERSION);

	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_push_ui_translation(obs_module_get_string);
	auto dockContents = new SceneNotesDock(main_window);

#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 0, 0)
	obs_frontend_add_dock_by_id(
		"SceneNotesDock", obs_module_text("SceneNotes"), dockContents);
#else
	const auto dock = new QDockWidget(main_window);
	dock->setFeatures(QDockWidget::DockWidgetMovable |
			  QDockWidget::DockWidgetFloatable);
	dock->setWindowTitle(QT_UTF8(obs_module_text("SceneNotes")));
	dock->setObjectName("SceneNotesDock");
	dock->setFloating(true);
	dock->hide();
	dock->setWidget(dockContents);
	obs_frontend_add_dock(dock);
#endif
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

static void frontend_event(enum obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
		auto *dock = static_cast<SceneNotesDock *>(data);
		dock->LoadNotes();
	}
}

void frontend_save(obs_data_t *save_data, bool saving, void *data)
{
	auto *dock = static_cast<SceneNotesDock *>(data);
	if (saving) {
		obs_data_array_t *hotkey_save_array =
			obs_hotkey_save(dock->insertTime);
		obs_data_set_array(save_data, "sceneNotesDockInsertTimeHotkey",
				   hotkey_save_array);
		obs_data_array_release(hotkey_save_array);
		obs_data_set_bool(save_data, "notes_auto_scroll",
				  dock->timer.isActive());
		obs_data_set_int(save_data, "notes_scroll_speed",
				 dock->timer.interval());
		obs_data_array_t *hotkey_start_save_array = nullptr;
		obs_data_array_t *hotkey_stop_save_array = nullptr;
		obs_hotkey_pair_save(dock->toggleAutoScroll,
				     &hotkey_start_save_array,
				     &hotkey_stop_save_array);
		if (hotkey_start_save_array) {
			obs_data_set_array(
				save_data,
				"sceneNotesDockAutoStartScrollHotkey",
				hotkey_start_save_array);
			obs_data_array_release(hotkey_start_save_array);
		}
		if (hotkey_stop_save_array) {
			obs_data_set_array(save_data,
					   "sceneNotesDockAutoStopScrollHotkey",
					   hotkey_stop_save_array);
			obs_data_array_release(hotkey_stop_save_array);
		}
	} else {
		obs_data_array_t *hotkey_save_array = obs_data_get_array(
			save_data, "sceneNotesDockInsertTimeHotkey");
		obs_hotkey_load(dock->insertTime, hotkey_save_array);
		obs_data_array_release(hotkey_save_array);
		int speed = obs_data_get_int(save_data, "notes_scroll_speed");
		if (speed)
			dock->timer.setInterval(speed);
		if (obs_data_get_bool(save_data, "notes_auto_scroll")) {
			if (!dock->timer.isActive())
				dock->timer.start();
		} else if (dock->timer.isActive()) {
			dock->timer.stop();
		}
		obs_data_array_t *hotkey_start_save_array = obs_data_get_array(
			save_data, "sceneNotesDockAutoStartScrollHotkey");
		obs_data_array_t *hotkey_stop_save_array = obs_data_get_array(
			save_data, "sceneNotesDockAutoStartScrollHotkey");
		obs_hotkey_pair_load(dock->toggleAutoScroll,
				     hotkey_start_save_array,
				     hotkey_stop_save_array);
		obs_data_array_release(hotkey_start_save_array);
		obs_data_array_release(hotkey_stop_save_array);
	}
}

static void InsertTimePressed(void *data, obs_hotkey_id id,
			      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (!pressed)
		return;
	auto dock = static_cast<SceneNotesDock *>(data);
	QMetaObject::invokeMethod(dock, "InsertTime");
}

static bool StartAutoScrollPressed(void *data, obs_hotkey_pair_id id,
				   obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (!pressed)
		return false;
	auto dock = static_cast<SceneNotesDock *>(data);
	if (dock->timer.isActive())
		return false;
	dock->timer.start();
	return true;
}

static bool StopAutoScrollPressed(void *data, obs_hotkey_pair_id id,
				  obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (!pressed)
		return false;
	auto dock = static_cast<SceneNotesDock *>(data);
	if (!dock->timer.isActive())
		return false;
	dock->timer.stop();
	return true;
}

SceneNotesDock::SceneNotesDock(QWidget *parent)
	: QWidget(parent),
	  show_preview(config_get_bool(obs_frontend_get_global_config(),
				       "SceneNotesDock", "ShowPreview")),
	  textEdit(new QTextEdit(this)),
	  insertTime(obs_hotkey_register_frontend(
		  "SceneNotesDockInsertTime",
		  obs_module_text("SceneNotesDockInsertTime"),
		  InsertTimePressed, this)),
	  toggleAutoScroll(obs_hotkey_pair_register_frontend(
		  "SceneNotesDockStartAutoScroll",
		  obs_module_text("SceneNotesDockStartAutoScroll"),
		  "SceneNotesDockStopAutoScroll",
		  obs_module_text("SceneNotesDockStopAutoScroll"),
		  StartAutoScrollPressed, StopAutoScrollPressed, this, this)),
	  timer()
{
	auto *mainLayout = new QVBoxLayout(this);

	mainLayout->addWidget(textEdit);

	setLayout(mainLayout);

	auto changeText = [this]() {
		obs_source_t *scene =
			show_preview && obs_frontend_preview_program_mode_active()
				? obs_frontend_get_current_preview_scene()
				: obs_frontend_get_current_scene();
		if (!scene)
			return;
		QString old_notes;
		if (auto *settings = obs_source_get_settings(scene)) {
			auto file = obs_data_get_string(settings, "notes_file");
			if (file && strlen(file) && os_file_exists(file)) {
				auto html = os_quick_read_utf8_file(file);
				old_notes = QT_UTF8(html);
				bfree(html);
			} else {
				old_notes = QT_UTF8(
					obs_data_get_string(settings, "notes"));
			}
			const auto new_notes = textEdit->toHtml();
			if (old_notes != new_notes) {
				if (file && strlen(file)) {
					auto h = new_notes.toUtf8();
					auto html = h.constData();
					if (os_quick_write_utf8_file(
						    file, html, strlen(html),
						    false)) {
						auto item = obs_data_item_byname(
							settings, "notes");
						if (item) {
							obs_data_item_remove(
								&item);
							obs_data_item_release(
								&item);
						}
					}
				} else {
					obs_data_set_string(
						settings, "notes",
						QT_TO_UTF8(new_notes));
				}
			}
			obs_data_release(settings);
		}
		obs_source_release(scene);
	};
	connect(textEdit, &QTextEdit::textChanged, changeText);

	textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
	auto contextMenu = [this]() {
		auto *menu = textEdit->createStandardContextMenu();
		if (!textEdit->isReadOnly()) {
			menu->addSeparator();
			auto setFont = [this]() {
				bool success = false;
				const auto font = QFontDialog::getFont(
					&success, textEdit->currentFont(), this,
					QT_UTF8(obs_module_text("Font")));
				if (success)
					textEdit->setCurrentFont(font);
			};
			menu->addAction(QT_UTF8(obs_module_text("Font")), this,
					setFont);

			auto setFontColor = [this]() {
				const QColor newColor = QColorDialog::getColor(
					textEdit->textColor(), this,
					QT_UTF8(obs_module_text("TextColor")));
				if (newColor.isValid()) {
					textEdit->setTextColor(newColor);
				}
			};
			menu->addAction(QT_UTF8(obs_module_text("TextColor")),
					this, setFontColor);
			auto setBackgroundColor = [this]() {
				const QColor newColor = QColorDialog::getColor(
					textEdit->textBackgroundColor(), this,
					QT_UTF8(obs_module_text(
						"BackGroundColor")));
				if (newColor.isValid()) {
					textEdit->setTextBackgroundColor(
						newColor);
				}
			};
			menu->addAction(
				QT_UTF8(obs_module_text("BackgroundColor")),
				this, setBackgroundColor);

			auto listMenu =
				menu->addMenu(QT_UTF8(obs_module_text("List")));

			std::vector<
				std::pair<QTextListFormat::Style, const char *>>
				types{
					{QTextListFormat::ListDisc, "Disc"},
					{QTextListFormat::ListCircle, "Circle"},
					{QTextListFormat::ListSquare, "Square"},
					{QTextListFormat::ListDecimal,
					 "Decimal"},
					{QTextListFormat::ListLowerAlpha,
					 "LowerAlpha"},
					{QTextListFormat::ListUpperAlpha,
					 "UpperAlpha"},
					{QTextListFormat::ListLowerRoman,
					 "LowerRoman"},
					{QTextListFormat::ListUpperRoman,
					 "UpperRoman"},
				};
			for (const auto &it : types) {
				auto t = it.first;
				auto setListType = [this, t]() {
					auto cursor = textEdit->textCursor();
					auto cl = cursor.currentList();
					if (!cl) {
						cursor.createList(t);
					} else {
						auto f = cl->format();
						f.setStyle(t);
						cl->setFormat(f);
					}
				};
				auto a = listMenu->addAction(
					QT_UTF8(obs_module_text(it.second)),
					this, setListType);
				a->setCheckable(true);
				auto cursor = textEdit->textCursor();
				auto cl = cursor.currentList();
				if (cl && cl->format().style() == it.first) {
					a->setChecked(true);
				}
			}

			listMenu->addSeparator();

			auto setListIncr = [this]() {
				auto cursor = textEdit->textCursor();
				QTextBlockFormat blockFormat =
					cursor.block().blockFormat();
				blockFormat.setIndent(blockFormat.indent() + 1);
				cursor.beginEditBlock();
				cursor.setBlockFormat(blockFormat);
				auto cl = cursor.currentList();
				if (cl) {
					cursor.createList(cl->format().style());
				}
				cursor.endEditBlock();
			};
			listMenu->addAction(
				QT_UTF8(obs_module_text("IncreaseIndent")),
				this, setListIncr);

			auto setListDecr = [this]() {
				auto cursor = textEdit->textCursor();
				auto block = cursor.block();
				QTextBlockFormat blockFormat =
					block.blockFormat();
				auto i = blockFormat.indent();
				if (i <= 0) {
					auto cl = block.textList();
					if (cl) {
						cursor.beginEditBlock();
						const auto count = cl->count();
						for (int i = 0; i < count; i++)
							cl->removeItem(0);
						cursor.endEditBlock();
					}
					return;
				}
				cursor.beginEditBlock();
				blockFormat.setIndent(i - 1);
				cursor.setBlockFormat(blockFormat);
				block = cursor.block();
				if (auto cl = block.textList()) {
					auto p = block.previous();
					auto ptll = p.textList();
					if (ptll && p.blockFormat().indent() ==
							    block.blockFormat()
								    .indent()) {
						auto count = cl->count();
						for (int i = 0; i < count;
						     i++) {
							auto item = cl->item(0);
							if (ptll) {
								ptll->add(item);
							} else {
								cl->remove(
									item);
							}
						}
					}
				}
				cursor.endEditBlock();
			};
			listMenu->addAction(
				QT_UTF8(obs_module_text("DecreaseIndent")),
				this, setListDecr);

			menu->addSeparator();
			auto clearFormat = [this]() {
				const auto text = textEdit->toPlainText();
				textEdit->setTextColor(
					textEdit->palette().color(
						QPalette::ColorRole::Text));
				textEdit->setTextBackgroundColor(
					textEdit->palette().color(
						QPalette::ColorRole::Base));
				textEdit->setCurrentFont(textEdit->font());
				textEdit->setPlainText(text);
			};
			menu->addAction(QT_UTF8(obs_module_text("ClearFormat")),
					this, clearFormat);
		}
		menu->addSeparator();
		auto a = menu->addAction(
			QT_UTF8(obs_module_text("ShowPreview")), this, [this] {
				show_preview = !show_preview;
				config_set_bool(
					obs_frontend_get_global_config(),
					"SceneNotesDock", "ShowPreview",
					show_preview);
				LoadNotes();
			});
		a->setCheckable(true);
		a->setChecked(show_preview);
		a->setEnabled(obs_frontend_preview_program_mode_active());
		a = menu->addAction(QT_UTF8(obs_module_text("Locked")), this, [this] {
			textEdit->setReadOnly(!textEdit->isReadOnly());
			obs_source_t *scene =
				show_preview && obs_frontend_preview_program_mode_active()
					? obs_frontend_get_current_preview_scene()
					: obs_frontend_get_current_scene();
			if (!scene)
				return;
			if (auto *settings = obs_source_get_settings(scene)) {
				obs_data_set_bool(settings, "notes_locked",
						  textEdit->isReadOnly());
				obs_data_release(settings);
			}
			obs_source_release(scene);
		});
		a->setCheckable(true);
		a->setChecked(textEdit->isReadOnly());
		menu->addSeparator();
		a = menu->addAction(
			QT_UTF8(obs_module_text("SaveInSceneCollection")), this,
			[this] {
				obs_source_t *scene =
					show_preview && obs_frontend_preview_program_mode_active()
						? obs_frontend_get_current_preview_scene()
						: obs_frontend_get_current_scene();
				if (!scene)
					return;
				if (auto *settings =
					    obs_source_get_settings(scene)) {
					obs_data_set_string(settings,
							    "notes_file", "");
					const auto notes = textEdit->toHtml();
					obs_data_set_string(settings, "notes",
							    QT_TO_UTF8(notes));
					obs_data_release(settings);
				}
				obs_source_release(scene);
			});
		a->setCheckable(true);
		const char *file = nullptr;
		obs_source_t *scene =
			show_preview && obs_frontend_preview_program_mode_active()
				? obs_frontend_get_current_preview_scene()
				: obs_frontend_get_current_scene();
		if (!scene)
			return;
		if (auto *settings = obs_source_get_settings(scene)) {
			file = obs_data_get_string(settings, "notes_file");
			obs_data_release(settings);
		}
		obs_source_release(scene);
		a->setChecked(!file || !strlen(file));

		a = menu->addAction(QT_UTF8(obs_module_text("SaveInFile")), this, [this] {
			obs_source_t *scene =
				show_preview && obs_frontend_preview_program_mode_active()
					? obs_frontend_get_current_preview_scene()
					: obs_frontend_get_current_scene();
			if (!scene)
				return;
			if (auto *settings = obs_source_get_settings(scene)) {
				auto file = obs_data_get_string(settings,
								"notes_file");

				QString fileName = QFileDialog::getSaveFileName(
					this, "", file, "HTML File (*.html)");
				if (!fileName.isEmpty()) {
					obs_data_set_string(
						settings, "notes_file",
						QT_TO_UTF8(fileName));
					auto html = os_quick_read_utf8_file(
						QT_TO_UTF8(fileName));
					if (html) {
						textEdit->setHtml(
							QT_UTF8(html));
						bfree(html);
					} else {
						auto h = textEdit->toHtml()
								 .toUtf8();
						auto html = h.constData();
						os_quick_write_utf8_file(
							QT_TO_UTF8(fileName),
							html, strlen(html),
							false);
					}
				}
				obs_data_release(settings);
			}
			obs_source_release(scene);
		});
		a->setCheckable(true);
		a->setChecked(file && strlen(file));
		menu->addSeparator();
		a = menu->addAction(QT_UTF8(obs_module_text("AutoScroll")),
				    this, [this] {
					    if (timer.isActive()) {
						    timer.stop();
					    } else {
						    timer.start();
					    }
				    });
		a->setCheckable(true);
		a->setChecked(timer.isActive());

		QSlider *speed = new QSlider(Qt::Horizontal, menu);
		speed->setMinimum(0);
#define MAX_SCROLL_SPEED 500
		speed->setMaximum(MAX_SCROLL_SPEED - 1);
		speed->setSingleStep(1);
		speed->setValue(MAX_SCROLL_SPEED - timer.interval());

		auto setSpeed = [this](int speed) {
			if (timer.interval() == MAX_SCROLL_SPEED - speed)
				return;
			bool active = timer.isActive();
			if (active)
				timer.stop();
			timer.setInterval(MAX_SCROLL_SPEED - speed);
			if (active)
				timer.start();
		};
		connect(speed, (void(QSlider::*)(int)) & QSlider::valueChanged,
			setSpeed);

		QWidgetAction *speedAction = new QWidgetAction(menu);
		speedAction->setDefaultWidget(speed);

		menu->addAction(speedAction);

		menu->addSeparator();
		auto about_menu = menu->addMenu(
			QString::fromUtf8(obs_module_text("About")));
		about_menu->addAction(
			QString::fromUtf8("Scene Notes Dock (") +
				PROJECT_VERSION + ")",
			[] {
				QDesktopServices::openUrl(QUrl(
					"https://obsproject.com/forum/resources/scene-notes-dock.1398/"));
			});
		about_menu->addAction(QString::fromUtf8("Exeldro"), [] {
			QDesktopServices::openUrl(QUrl("https://exeldro.com"));
		});

		menu->exec(QCursor::pos());
	};
	connect(textEdit, &QTextEdit::customContextMenuRequested, contextMenu);
	connect(&timer, &QTimer::timeout, [this] {
		auto vs = textEdit->verticalScrollBar();
		if (vs->value() < vs->maximum()) {
			vs->setValue(vs->value() + 1);
		}
	});
	obs_frontend_add_event_callback(frontend_event, this);
	obs_frontend_add_save_callback(frontend_save, this);
}

SceneNotesDock::~SceneNotesDock()
{
	obs_hotkey_unregister(insertTime);
	obs_frontend_remove_save_callback(frontend_save, this);
	obs_frontend_remove_event_callback(frontend_event, this);
}

void SceneNotesDock::LoadNotes()
{
	obs_source_t *scene =
		show_preview && obs_frontend_preview_program_mode_active()
			? obs_frontend_get_current_preview_scene()
			: obs_frontend_get_current_scene();
	if (!scene)
		return;

	if (auto *settings = obs_source_get_settings(scene)) {
		auto file = obs_data_get_string(settings, "notes_file");
		if (file && strlen(file) && os_file_exists(file)) {
			auto html = os_quick_read_utf8_file(file);
			textEdit->setHtml(QT_UTF8(html));
			bfree(html);
		} else {
			textEdit->setHtml(QT_UTF8(
				obs_data_get_string(settings, "notes")));
		}
		textEdit->setReadOnly(
			obs_data_get_bool(settings, "notes_locked"));
		obs_data_release(settings);
	}
	obs_source_release(scene);
}

void SceneNotesDock::InsertTime()
{
	time_t rawtime;
	struct tm *timeinfo;
	char buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer, 80, "%X ", timeinfo);
	textEdit->insertPlainText(QT_UTF8(buffer));
}
