/* Skittle-Qt (2022) http://github.com/dualword/Skittle-Qt License:GNU GPL*/
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QSpinBox>
#include <QString>
#include <sstream>
#include <algorithm>
#include <QDebug>
#include "UiVariables.h"

using std::max;
/** *******************************
This class is a container class for the set of 5 global dials that affect all
Graph visualizations.  Those dials are: start, width, size, scale, and zoom.
It also contains a pointer to the local offsetDial for multiple windows.
Finally, UiVariables has a pointer to the textArea of "Information Display"
and convenience functions for printing out information to the text area.  This
is useful for writing debugging notes since cout is generally unavailable.
You should also use the print functions to provide data to the user in a format
that can be copy and pasted without the need to write a file.
**********************************/

// Global static pointer used to ensure a single instance of the class.
UiVariables* UiVariables::pointerInstance = NULL;


UiVariables::UiVariables(QTextEdit* text)
{
    textArea = text;
    if(textArea != NULL)
        textArea->toPlainText();

    widthDial = new QSpinBox();
    widthDial->setMinimum(1);
    widthDial->setMaximum(1000000000);
    widthDial->setValue(128);
    widthDial->setSuffix(" bp");
    widthDial->setButtonSymbols(QAbstractSpinBox::NoButtons);

    scaleDial = new QSpinBox();
    scaleDial->setMinimum(1);
    scaleDial->setMaximum(100000);
    scaleDial->setValue(1);
    scaleDial->setSingleStep(4);
    scaleDial->setSuffix(" bp/pixel");
    scaleDial->setButtonSymbols(QAbstractSpinBox::NoButtons);

    zoomDial = new QSpinBox();
    zoomDial->setMinimum(1);
    zoomDial->setMaximum(100000);
    zoomDial->setSingleStep(10);
    zoomDial->setValue(100);
    zoomDial->setButtonSymbols(QAbstractSpinBox::NoButtons);

    startDial = new QSpinBox();
    startDial->setMinimum(1);
    startDial->setMaximum(400000000);
    startDial->setValue(1);
    startDial->setButtonSymbols(QAbstractSpinBox::NoButtons);

    sizeDial = new QSpinBox();
    sizeDial->setMinimum(1000);
    sizeDial->setMaximum(400000000);//something very large MAX_INT?
    sizeDial->setSingleStep(1000);
    sizeDial->setValue(10000);
    sizeDial->setSuffix(" bp");
    sizeDial->setButtonSymbols(QAbstractSpinBox::NoButtons);

    oldScale = 1;
    oldWidth = 128;

    colorSetting = CLASSIC; // default

    //These lines make the Graphs respond immediately to typing rather than waiting for the user to finish
    /*connect(widthDial, SIGNAL(valueChanged(int)), this, SIGNAL(internalsUpdated()));
    connect(scaleDial, SIGNAL(valueChanged(int)), this, SIGNAL(internalsUpdated()));
    connect(zoomDial, SIGNAL(valueChanged(int)), this, SIGNAL(internalsUpdated()));
    connect(startDial, SIGNAL(valueChanged(int)), this, SIGNAL(internalsUpdated()));*/

    connect(widthDial, SIGNAL(editingFinished()), this, SIGNAL(internalsUpdated()));
    connect(scaleDial, SIGNAL(editingFinished()), this, SIGNAL(internalsUpdated()));
    connect(zoomDial,  SIGNAL(editingFinished()), this, SIGNAL(internalsUpdated()));
    connect(startDial, SIGNAL(editingFinished()), this, SIGNAL(internalsUpdated()));
    connect(sizeDial,  SIGNAL(editingFinished()), this, SIGNAL(internalsUpdated()));
}

UiVariables::~UiVariables()
{
    //delete all the individual dial pointers?
}

/** *********************
  Singleton design pattern: Making the constructor private at Instance() static
  ensures that there is only ever one UiVariables.
  ************************/
UiVariables* UiVariables::Instance()//static
{
    if(pointerInstance == NULL)
        pointerInstance = new UiVariables();
    return pointerInstance;
}

void UiVariables::newOffsetDial(GLWidget* gl)
{
    QSpinBox* offsetDial = new QSpinBox();
    offsetDial->setMinimum(-40000000);
    offsetDial->setMaximum(40000000);
    offsetDial->setValue(0);
    offsetDial->setSingleStep(1);
    offsets[gl] = offsetDial;

    connect(offsetDial, SIGNAL(editingFinished()), this, SIGNAL(internalsUpdated()));
}

QSpinBox* UiVariables::getOffsetDial(GLWidget* gl)
{
    QSpinBox* r = offsets[gl];
    return r;
}

void UiVariables::print(char const* s)
{
    if(textArea != NULL)
    {
        QString name(s);
        textArea->append(name);
    }
}

void UiVariables::print(std::string s)
{
    print(s.c_str());
}

void UiVariables::printHtml(std::string s)
{
    if(textArea != NULL)
        textArea->insertHtml(QString(s.c_str()));
}

void UiVariables::print(const char* s, int num)
{
    std::stringstream ss1;
    ss1 << s << num;

    print( ss1.str().c_str() );
}

void UiVariables::printNum(int num)
{
    std::stringstream ss1;
    ss1 << num;

    print(ss1.str().c_str() );
}

void UiVariables::setAllVariables(int width, int scale, int zoom, int start, int size)
{
    //TODO: add validity checking
    if(width != -1)
        widthDial->setValue(width);
    if(scale != -1)
    {
        if(width == -1)
            setScale(scale);
        else
            scaleDial->setValue(scale);
    }
    if(zoom != -1)
        zoomDial->setValue(zoom);
    if(start != -1)
        startDial->setValue(start);
    if(size != -1)
        sizeDial->setValue(size);

    emit internalsUpdated();
}

int UiVariables::getWidth()
{
    return widthDial->value();
}

void UiVariables::setWidth(int newWidth)
{
    if(newWidth < 1)
        newWidth = 1;

    if (newWidth != oldWidth)
    {
        int newScale = oldScale;
        int displayWidth = newWidth / oldScale;
        if ( displayWidth < 1 || displayWidth > maxSaneWidth)
        {
            newScale = int(max(double(1.0), double(oldScale) * ( double(newWidth) / double(oldWidth))));
            scaleDial->setValue(newScale);
        }
        widthDial->setValue(newWidth);
        sizeDial->setSingleStep(newWidth*10);
        oldWidth = newWidth;
        oldScale = newScale;
        emit internalsUpdated();
    }
}

int UiVariables::getScale()
{
    return scaleDial->value();
}

void UiVariables::setScale(int newScale)
{
    if(newScale < 1)
        newScale = 1;

    if(scaleDial->value() != newScale)
    {
        int display_width = max( 1, getWidth() / scaleDial->value());

        int display_size = sizeDial->value() / scaleDial->value();
        display_size = max( 1, display_size);
        int newWidth = display_width * newScale;
        widthDial->setValue( newWidth);
        scaleDial->setValue(newScale);
        sizeDial->setMinimum(scaleDial->value() * 500);
        sizeDial->setValue(display_size*newScale);

        widthDial->setSingleStep(newScale);//increment the width step by scaleDial
        widthDial->setMinimum(newScale);
        widthDial->setMaximum(maxSaneWidth * newScale);
        oldScale = newScale;
        oldWidth = newWidth;
        emit internalsUpdated();
    }
}

int UiVariables::getStart(GLWidget* gl)
{
    QSpinBox* dial = getOffsetDial(gl);
    if(dial)
    {
        return max(1, startDial->value() + dial->value());
    }
    return startDial->value();
}

void UiVariables::setStart(GLWidget* saysWho, int start)
{
    QSpinBox* dial = getOffsetDial(saysWho);
    if(dial)
    {
        int newStart = max(1, start - dial->value() );
        if(valueIsGoingToChange(startDial, newStart))
        {
            startDial->setValue(newStart);
            emit internalsUpdated();
        }
    }
}

int UiVariables::getZoom()
{
    return zoomDial->value();
}

void UiVariables::setZoom(int zoom)
{
    if(valueIsGoingToChange(zoomDial, zoom))
    {
        zoomDial->setValue(zoom);
        emit internalsUpdated();
    }
}

int UiVariables::getSize()
{
    return sizeDial->value();
}

void UiVariables::setSize(int size)
{
    if(valueIsGoingToChange(sizeDial, size))
    {
        sizeDial->setValue(size);
        emit internalsUpdated();
    }
}

void UiVariables::setOffsetDelta(GLWidget* gl, int deltaO)
{
    if(deltaO != 0)
    {
        QSpinBox* offsetDial = offsets[gl];
        if(offsetDial)
        {
            int offset = offsetDial->value() + deltaO;
            offsetDial->setValue(offset);

            emit internalsUpdated();
        }
    }
}

bool UiVariables::valueIsGoingToChange(QSpinBox* dial, int val)
{
    return (val != dial->value() &&
            val >= dial->minimum() &&
            val <= dial->maximum());
}

int UiVariables::getColorSetting()
{
    return colorSetting;
}

void UiVariables::changeColorSetting(int newColorSetting)
{
    colorSetting = newColorSetting;
    //we are not doing validity checking because currently the only thing that accesses this is
    //a MainWindow call that is hard coded to the values of enum colorPalettes
    //setting the signal/slot architecture to using enum colorPalettes instead of int
    //would require creating an extra slot in MainWindow
    emit colorsChanged(newColorSetting);
}

vector<QSpinBox*> UiVariables::getDialPointers()
{
    vector<QSpinBox*> dials;
    dials.push_back( widthDial );
    dials.push_back(scaleDial );
    dials.push_back(zoomDial );
    dials.push_back(startDial );
    dials.push_back(sizeDial );

    return dials;
}
