#pragma once
#include "obs.h"
#include <obs-frontend-api.h>
#include <QDockWidget>
#include <QTextEdit>
#include "obs.hpp"

class SceneNotesDock : public QDockWidget {
	Q_OBJECT

private:
private slots:

public:
	SceneNotesDock(QWidget *parent = nullptr);
	~SceneNotesDock();
	QTextEdit *textEdit;
};
