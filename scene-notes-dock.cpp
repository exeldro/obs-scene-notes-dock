#include "scene-notes-dock.hpp"
#include <obs-module.h>
#include <QColorDialog>
#include <QFontDialog>
#include <QMainWindow>
#include <QMenu>
#include <QStyle>
#include <QVBoxLayout>
#include <QTextList>

#include "version.h"
#include "util/config-file.h"

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
		auto *dock = static_cast<SceneNotesDock *>(data);
		dock->LoadNotes();
	}
}

SceneNotesDock::SceneNotesDock(QWidget *parent)
	: QDockWidget(parent),
	  textEdit(new QTextEdit(this)),
	  show_preview(config_get_bool(obs_frontend_get_global_config(),
				       "SceneNotesDock", "ShowPreview"))
{
	setFeatures(DockWidgetMovable | DockWidgetFloatable);
	setWindowTitle(QT_UTF8(obs_module_text("SceneNotes")));
	setObjectName("SceneNotesDock");
	setFloating(true);
	hide();

	auto *mainLayout = new QVBoxLayout(this);

	mainLayout->addWidget(textEdit);

	auto *dockWidgetContents = new QWidget;
	dockWidgetContents->setLayout(mainLayout);
	setWidget(dockWidgetContents);

	auto changeText = [this]() {
		obs_source_t *scene =
			show_preview && obs_frontend_preview_program_mode_active()
				? obs_frontend_get_current_preview_scene()
				: obs_frontend_get_current_scene();
		if (!scene)
			return;
		if (auto *settings = obs_source_get_settings(scene)) {
			const auto old_notes =
				QT_UTF8(obs_data_get_string(settings, "notes"));
			const auto new_notes = textEdit->toHtml();
			if (old_notes != new_notes) {
				obs_data_set_string(settings, "notes",
						    QT_TO_UTF8(new_notes));
			}
			obs_data_release(settings);
		}
		obs_source_release(scene);
	};
	connect(textEdit, &QTextEdit::textChanged, changeText);

	textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
	auto contextMenu = [this]() {
		auto *menu = textEdit->createStandardContextMenu();
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
		menu->addAction(QT_UTF8(obs_module_text("TextColor")), this,
				setFontColor);
		auto setBackgroundColor = [this]() {
			const QColor newColor = QColorDialog::getColor(
				textEdit->textBackgroundColor(), this,
				QT_UTF8(obs_module_text("BackGroundColor")));
			if (newColor.isValid()) {
				textEdit->setTextBackgroundColor(newColor);
			}
		};
		menu->addAction(QT_UTF8(obs_module_text("BackgroundColor")),
				this, setBackgroundColor);

		auto listMenu = menu->addMenu(QT_UTF8(obs_module_text("List")));

		std::vector<std::pair<QTextListFormat::Style, const char *>>
			types{
				{QTextListFormat::ListDisc, "Disc"},
				{QTextListFormat::ListCircle, "Circle"},
				{QTextListFormat::ListSquare, "Square"},
				{QTextListFormat::ListDecimal, "Decimal"},
				{QTextListFormat::ListLowerAlpha, "LowerAlpha"},
				{QTextListFormat::ListUpperAlpha, "UpperAlpha"},
				{QTextListFormat::ListLowerRoman, "LowerRoman"},
				{QTextListFormat::ListUpperRoman, "UpperRoman"},
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
			listMenu->addAction(QT_UTF8(obs_module_text(it.second)),
					    this, setListType);
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
		listMenu->addAction(QT_UTF8(obs_module_text("IncreaseIndent")),
				    this, setListIncr);

		auto setListDecr = [this]() {
			auto cursor = textEdit->textCursor();
			auto block = cursor.block();
			QTextBlockFormat blockFormat = block.blockFormat();
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
				if (ptll &&
				    p.blockFormat().indent() ==
					    block.blockFormat().indent()) {
					auto count = cl->count();
					for (int i = 0; i < count; i++) {
						auto item = cl->item(0);
						if (ptll) {
							ptll->add(item);
						} else {
							cl->remove(item);
						}
					}
				}
			}
			cursor.endEditBlock();
		};
		listMenu->addAction(QT_UTF8(obs_module_text("DecreaseIndent")),
				    this, setListDecr);

		menu->addSeparator();
		auto clearFormat = [this]() {
			const auto text = textEdit->toPlainText();
			textEdit->setTextColor(textEdit->palette().color(
				QPalette::ColorRole::Foreground));
			textEdit->setTextBackgroundColor(
				textEdit->palette().color(
					QPalette::ColorRole::Background));
			textEdit->setCurrentFont(textEdit->font());
			textEdit->setPlainText(text);
		};
		menu->addAction(QT_UTF8(obs_module_text("ClearFormat")), this,
				clearFormat);

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

		menu->exec(QCursor::pos());
	};
	connect(textEdit, &QTextEdit::customContextMenuRequested, contextMenu);

	obs_frontend_add_event_callback(frontend_event, this);
}

SceneNotesDock::~SceneNotesDock()
{
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
		textEdit->setHtml(
			QT_UTF8(obs_data_get_string(settings, "notes")));
		obs_data_release(settings);
	}
	obs_source_release(scene);
}
