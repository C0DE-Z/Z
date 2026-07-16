#ifndef APPSTATE_H
#define APPSTATE_H

#include <QObject>
#include <QList>
#include <QJsonObject>
#include "project.h"

class AppState : public QObject {
    Q_OBJECT

public:
    static AppState& instance() {
        static AppState inst;
        return inst;
    }

    void pushUndoState();
    bool undo();
    bool redo();
    void clear();

    double currentPlayhead() const { return playhead; }
    void setPlayhead(double time) { playhead = time; }

    bool isPlaying() const { return playing; }
    void setPlaying(bool p) { playing = p; }

signals:
    void stateRestored(); // emitted after undo/redo

private:
    AppState() = default;
    
    QList<QJsonObject> undoStack;
    QList<QJsonObject> redoStack;

    double playhead = 0.0;
    bool playing = false;
};

#endif // APPSTATE_H
