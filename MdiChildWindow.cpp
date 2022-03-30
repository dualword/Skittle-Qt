/* Skittle-Qt (2022) http://github.com/dualword/Skittle-Qt License:GNU GPL*/
#include "MdiChildWindow.h"
#include "glwidget.h"
#include <QtWidgets/QTabWidget>
#include <algorithm>

using std::find;

/** ************************************************
MdiChildWindow acts as the interface between ViewManager and GLWidget. It means that a subwindow
inside of skittle can be minimized, restored, and closed.  Multiple files can be laid out side by
side for comparison.  ViewManager and MdiChildWindow handle the start offset between
windows.  Each window that is created makes a new ui->offsetDial that manages the relative offset between
the local and global start position (positive or negative).  The other function of MdiChildWindow
is to ensure that the correct settings tabs for the active window are displayed on the Information Display
QDockWidget *infoDock.

Window Hierarchy:
MainWindow -> (1) Viewmanager -> (many) MdiChildWindow -> (1) GLWidget -> (1)FastaReader -> (1)File
MainWindow is at the top of a window hierarchy.  The center widget of MainWindow is a single
ViewManager which inherits from MdiArea.  The multiple document interface (MDI) can have multiple
MdiChildWindows.  Each MdiChildWindow has only one GLWidget tied to one file.  MainWindow is
the primary owner of the UiVariables object that is passed for signals all throughout the program.
*************************************************/

MdiChildWindow::MdiChildWindow(QTabWidget *settings)//TODO: use offsetDialIndex in ui.getStart()
    :QFrame()
{
    ui = UiVariables::Instance();
    settingsDock = settings;
    horizontalScrollBar = new QScrollBar();
    horizontalScrollBar->setOrientation(Qt::Horizontal);
    horizontalScrollBar->setMaximum (50);
    horizontalScrollBar->setPageStep(100);
    horizontalScrollBar->setSingleStep(10);
    verticalScrollBar = new QScrollBar();
    verticalScrollBar->setMaximum( 100 );
    verticalScrollBar->setTracking(0);

    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    subFrame = new QFrame(this);
    glWidget = new GLWidget(ui, this);
    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->addWidget(glWidget);
    hLayout->addWidget(verticalScrollBar);
    subFrame->setLayout(hLayout);//

    setWindowTitle("Skittle Logo");
    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->addWidget(subFrame);
    vLayout->addWidget(horizontalScrollBar);
    setLayout(vLayout);

    //scrollArea->setWidget(glWidget);//glWidget);
    setAttribute(Qt::WA_DeleteOnClose);
    
    /**Scrollbars**/
    connect(verticalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(changeStart(int)));
//    connect(verticalScrollBar, SIGNAL(valueChanged(int)), glWidget, SLOT(invalidateDisplayGraphs())); // 'cause setTracking(FALSE)
    connect(ui, SIGNAL(internalsUpdated()), this, SLOT(checkScrollBars()) );
    /**/
    createSettingsTabs();
    connectWidget();
    setPageSize();
}	

void MdiChildWindow::closeEvent(QCloseEvent *event)
{
    //QScrollBar* horizontalScrollBar;
    //QScrollBar* verticalScrollBar;
    //QFrame* subFrame;
    emit subWindowClosing(this);
    delete ui->getOffsetDial(glWidget);//->hide()
    delete glWidget;
    for(int i = 0; i < (int)settingsTabs.size(); ++i)
        delete settingsTabs[i];
    event->accept();
}

void MdiChildWindow::connectWidget()
{
    connect(horizontalScrollBar, SIGNAL(valueChanged(int)), glWidget, SLOT(slideHorizontal(int)));
    connect(glWidget, SIGNAL(xOffsetChange(int)), horizontalScrollBar, SLOT(setValue(int)));
    connect(glWidget, SIGNAL(totalWidthChanged(int)), this, SLOT(setHorizontalWidth(int)));
}

void MdiChildWindow::checkScrollBars()
{
    verticalScrollBar->setValue( ui->getStart(glWidget) );
    setPageSize();
    //TODO: move other scrollbar connections in here
}

void MdiChildWindow::changeStart(int val)
{
    ui->setStart(glWidget, val);
}

void MdiChildWindow::setHorizontalWidth(int val)
{
    horizontalScrollBar->setMaximum( max(0, val) );
}

void MdiChildWindow::setPageSize()
{
    //ui->print("setPageSize", ui->getSize());
    if( glWidget != NULL)
        verticalScrollBar->setMaximum( max(0, (int)(glWidget->seq()->size() - ui->getWidth()) ) );
    verticalScrollBar->setPageStep(ui->getSize());
}

void MdiChildWindow::createSettingsTabs()
{
    glWidget->settingsUi();
    connect( glWidget, SIGNAL(hideSettings(QScrollArea*)), this, SLOT(hideSettingsTab(QScrollArea*)));
    connect( glWidget, SIGNAL(showSettings(QScrollArea*)), this, SLOT(showSettingsTab(QScrollArea*)));
}

//NOTE: The plural and singular forms of (hide/show)SettingsTab(s) are similar but not identical in where they get the pointer
void MdiChildWindow::hideSettingsTab(QScrollArea* tab)
{
    int index = settingsDock->indexOf(tab);
    if(index > -1)
    {
        settingsDock->removeTab(index);
        vector<QScrollArea*>::iterator it;
        it = std::find(settingsTabs.begin(), settingsTabs.end(), tab);
        settingsTabs.erase(it);
    }
}
void MdiChildWindow::showSettingsTab(QScrollArea* tab)
{
    settingsTabs.push_back(tab);
    settingsDock->insertTab(1, tab, tab->windowTitle());
}

void MdiChildWindow::hideSettingsTabs()
{
    for(int i = 0; i < (int)settingsTabs.size(); ++i)
    {
        int index = settingsDock->indexOf(settingsTabs[i]);
        if(index > -1)
            settingsDock->removeTab(index);
        //settingsTabs[i]->removeTab(1);
    }
}

void MdiChildWindow::showSettingsTabs()
{
    for(int i = 0; i < (int)settingsTabs.size(); ++i)
    {
        settingsDock->insertTab(1, settingsTabs[i], settingsTabs[i]->windowTitle());
        //settingsTabs[i]->insertTab(1);
    }
}

void MdiChildWindow::mousePressEvent(QMouseEvent *event)
{
    //    qDebug("Window Clicked");
    emit IveBeenClicked(this);
}

