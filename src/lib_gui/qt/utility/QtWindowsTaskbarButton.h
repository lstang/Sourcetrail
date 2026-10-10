#ifndef QT_WINDOWS_TASKBAR_BUTTON_H
#define QT_WINDOWS_TASKBAR_BUTTON_H

class QtMainWindow;

class QtWindowsTaskbarButton
{
public:
	QtWindowsTaskbarButton();
	void setWindow(QtMainWindow* mainWindow);

	void setProgress(float progress);
	void hideProgress();

};
#endif	  // QT_WINDOWS_TASKBAR_BUTTON_H
