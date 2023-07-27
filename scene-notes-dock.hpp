#pragma once
#include "obs.h"
#include <obs-frontend-api.h>
#include <QDockWidget>
#include <QTextEdit>
#include "obs.hpp"

class SceneNotesDock : public QWidget {
	Q_OBJECT

private:
	bool show_preview;
	QTextEdit *textEdit;
private slots:
	void InsertTime();
public:
	SceneNotesDock(QWidget *parent = nullptr);
	~SceneNotesDock();
	void LoadNotes();
	obs_hotkey_id insertTime;
};
