/* Skittle-Qt (2022) http://github.com/dualword/Skittle-Qt License:GNU GPL*/
#include "HighlightDisplay.h"
#include "glwidget.h"
#include "SequenceEntry.h"
//#include <ctype.h>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QGridLayout>

using std::ifstream;

/** **************************************
  HighlightDisplay is a Graph class (AbstractGraph) that display a set of search sequences
  highlighted in their own colors.  All non-matching sequence is shown on grey-scale
  according to how closely they almost matched a search sequence ( max(match1, match2) ).
  The user can add new sequences to highlight through the settingsUi() tab.  By popular
  demand, the sequence highlighter also has the ability to read in an entire query file
  where each line is a search sequence.  This is accessed through the button OpenFileButton
  and the method openQueryFile().  Zooming out to higher scales offers a unique problem with
  HighlightDisplay since the useful information is sparse data.  The scaling method ensures
  that each identified sequence doesn't drop below 1 pixel unless it collides with a larger
  search sequence match.

  Development: Unlike NucleotideDisplay and RepeatMap, HighlightDisplay takes up linearly more
  processing time proportional to the size of the sequence.  It does not have any performance
  optimization for large sizes.  Adding a BLAST-like performance optimization would make this
  view more useful for mapping out genome element distributions.  Issue #32
****************************************/

HighlightDisplay::HighlightDisplay(UiVariables* gui, GLWidget* gl)
    :NucleotideDisplay(gui, gl)
{	
    hidden = true;
    actionLabel = string("Sequence Highlighter");
    actionTooltip = string("Highlights user specified sequences");
    actionData = actionLabel;
    activeSeqEdit = NULL;
    reverseCheck = NULL;
    formLayout = NULL;
    settingsBox = NULL;
    addButton = NULL;

    percentage_match = 0.8;
    frameCount = 0;
    rowCount = 0;
}

HighlightDisplay::~HighlightDisplay(){
    glDeleteLists(display_object, 1);
}

QScrollArea* HighlightDisplay::settingsUi()
{	
    settingsTab = new QScrollArea();
    settingsTab->setWindowTitle(QString("Sequence Highlighter Settings"));
    settingsTab->setMinimumHeight(120);
    settingsBox = new QFrame(settingsTab);
    settingsTab->setWidget(settingsBox);
    settingsTab->setWidgetResizable(true);
    
    formLayout = new QGridLayout(settingsBox);
    settingsBox->setLayout(formLayout);

    QSpinBox* similarityDial = new QSpinBox(settingsBox);
    similarityDial->setValue(80);
    similarityDial->setSuffix("%");
    similarityDial->setMinimum(20);
    similarityDial->setMaximum(100);
    reverseCheck = new QCheckBox("Search Reverse Complement", settingsBox);
    reverseCheck->setChecked(true);
    QPushButton* OpenFileButton = new QPushButton("Open Query File", settingsBox);
    QPushButton* clearEntriesButton = new QPushButton("Clear All", settingsBox);
    addButton = new QPushButton("Add a New Sequence", settingsBox);
    
    formLayout->addWidget(reverseCheck, 0,0);
    formLayout->addWidget(OpenFileButton, 0,1);
    formLayout->addWidget(clearEntriesButton, 0,2);
    formLayout->addWidget(new QLabel("Minimum Similarity:"), 1,0);
    formLayout->addWidget(similarityDial, 1,1);
    formLayout->addWidget(addButton, 2,0);
    addNewSequence();

    connect( addButton, SIGNAL(clicked()), this, SLOT(addNewSequence()));
    connect( OpenFileButton, SIGNAL(clicked()), this, SLOT(openQueryFile()));
    connect( clearEntriesButton, SIGNAL(clicked()), this, SLOT(clearAllEntries()));

    connect( similarityDial, SIGNAL(valueChanged(int)), this, SLOT(setPercentSimilarity(int)));
    connect( reverseCheck, SIGNAL(released()), this, SLOT(invalidate()));
    return settingsTab;
}

void HighlightDisplay::display()
{		
    checkVariables();
    glPushMatrix();
    glScaled(1,-1,1);
    if(!upToDate)
    {
        vector<vector<int> > results;
        for(int i = 0; i < (int)seqLines.size(); i++)
        {
            if( !seqLines[i]->seq.empty() )
            {
                results.push_back( identifyMatches( seqLines[i]->seq ) );
                if(reverseCheck->isChecked())
                    results.push_back( identifyMatches( reverseComplement(seqLines[i]->seq) ) );
            }
        }
        combine( results );
        loadTextureCanvas();
        upToDate = true;
    }
    if( !outputPixels.empty())
        textureBuffer->display();
    glPopMatrix();
}

GLuint HighlightDisplay::render()//deprecated
{
    GLuint list = glGenLists(1);
    return list;
}

vector<int> HighlightDisplay::identifyMatches(string find)
{
    vector<unsigned short int> scores = calculate(find);

    vector<int> pixels;
    int findSize = find.size();
    int remainingLength = 0;
    int match_minimum = (int)(255 * percentage_match);
    const char* seq = (sequence->c_str() + ui->getStart(glWidget));//TODO:not a particularly safe way of accessing
    int offset = 0;
    int tempScale = ui->getScale();
    for(int i = 0; i < (int)scores.size(); i+=tempScale)
    {
        vector<unsigned short int>::iterator bestMatch = max_element(scores.begin()+i, scores.begin()+i+tempScale);
        short int bestScore = *bestMatch;
        int grey = static_cast<int>(  float(bestScore)/findSize * 255 );//Grey scale based on similarity
        int pixelColor = grey;//white to grey

        //highlight matches with color
        if(grey >= match_minimum)//count this as a starting position
        {
            offset = distance(scores.begin()+i, bestMatch);
            remainingLength = findSize;// - scale;
        }
        if(remainingLength >= 1)//trail after a match
        {//green if it matches, blue if it doesn't
            if(seq[i+offset] == find[findSize - remainingLength])
                pixelColor = 260;//green
            else
                pixelColor = 258;//blue

            remainingLength = max(0, remainingLength - tempScale);
        }
        pixels.push_back( pixelColor );
    }
    return pixels;
}

//This calculates how well a region of the genome matches a query sequence 'find' at every nucleotide.  
vector<unsigned short int> HighlightDisplay::calculate(string find)
{
    vector<unsigned short int> scores;
    int findSize = find.size();

    int start = ui->getStart(glWidget);
    unsigned short int maxMismatches = findSize - static_cast<unsigned short int>((float)findSize * percentage_match + .999);
    //at 50%   1 = 0,  2 = 1, 3 = 1
    const string& seq = *sequence;
    for( int h = 0; h < current_display_size() && h  < (int)seq.size() - start - (findSize-1); h++)
    {
        unsigned short int mismatches = 0;
        int start_h = start + h;
        unsigned short int l = 0;
        while(mismatches <= maxMismatches && l < findSize)
        {
            if(seq[start_h + l] != find[l])//this is the innermost loop.  This line takes the most time
                ++mismatches;
            ++l;
        }
        scores.push_back(l - mismatches);
    }
    return scores;
}

void HighlightDisplay::combine(vector< vector<int> >& results)
{	
    outputPixels.clear();

    //ensure they're all the same length
    int nSequences = results.size();
    int length = current_display_size();
    if(!results.empty())
        length = results[0].size();
    vector< vector<int>::iterator > iterators;
    for(unsigned int i = 0; i < results.size(); ++i)
    {
        if( length != (int)results[i].size() )
        {
            results[i].resize(length, 0 );
        }
        iterators.push_back( results[i].begin() );
    }

    color c = color(0,0,0);
    int matchSeq = 0;
    for(int i = 0; i < length; i++)
    {
        int score = 0;
        for(int k = 0; k < nSequences; ++k)
        {
            score = max(score, results[k][i]);//*(iterators[k]));
            //score = *(iterators[k]);
            //++iterators[k];
            if(score > 256)
            {
                matchSeq = k;
                if(reverseCheck->isChecked())
                    matchSeq /= 2;
                break;
            }/**/
        }

        if(score < 257)
        {
            c = color(score, score, score);//grey pixel
        }
        else{
            if( score == 258) c = seqLines[matchSeq]->mismatchColor;
            if( score == 260) c = seqLines[matchSeq]->matchColor;
        }
        outputPixels.push_back( c );
    }
}

void HighlightDisplay::setHighlightSequence(const QString& high_C)
{
    if(seqLines.empty())
        addNewSequence();
    int index = seqLines.size()-1;
    seqLines[index]->setHighlightSequence(high_C);

    invalidate();
}
void HighlightDisplay::addNewSequence()
{
    if(seqLines.empty())
        addNewSequence(string("AAAAAAAAAAAA"));
    else
        addNewSequence(string());
}

void HighlightDisplay::addNewSequence(string startString)
{	
    activeSeqEdit = new QLineEdit(settingsBox);
    activeSeqEdit->setText(startString.c_str());
    std::stringstream ss1;
    ss1 << "Sequence " << ++rowCount;//Sequence number
    QLabel* label = new QLabel(ss1.str().c_str());
    SequenceEntry* entry = new SequenceEntry(label, activeSeqEdit );

    int row = formLayout->rowCount();
    formLayout->removeWidget(addButton);
    formLayout->addWidget(label, row, 0, Qt::AlignRight);
    formLayout->addWidget(activeSeqEdit, row, 1);
    formLayout->addWidget(entry->colorBox, row, 2);
    formLayout->addWidget(entry->removeButton, row, 3);
    formLayout->addWidget(addButton, row+1, 0);

    settingsBox->setMinimumHeight(settingsBox->minimumHeight() + activeSeqEdit->minimumHeight());
    seqLines.push_back( entry );

    connect(seqLines[seqLines.size()-1], SIGNAL( removeEntry(SequenceEntry*)), this, SLOT(removeEntry(SequenceEntry*)));
    connect(seqLines[seqLines.size()-1], SIGNAL( removeEntry(SequenceEntry*)), this, SLOT(invalidate()));
    connect( activeSeqEdit, SIGNAL(textChanged(const QString&)), this, SLOT(invalidate()));
    connect( entry, SIGNAL(colorChanged()), this, SLOT(invalidate()));

    upToDate = false;
}

string stripWhiteSpace(string line)
{
    string::iterator it = line.begin();
    for(; it < line.end(); ++it)
    {
        if(  *it == '\r' || (*it) == '\n' )
            it = line.erase(it);
    }
    return line;
}

void HighlightDisplay::openQueryFile()
{
    QString fileName = QFileDialog::getOpenFileName(
                this,"Open Query File",
                "",
                "All files (*)");//FASTA files (*.fa);;

    if (fileName.isEmpty())
        return;
    ui->print("Searching entries in: " + fileName.toStdString());

    ifstream file;
    file.open(fileName.toStdString().c_str(), ifstream::in | ifstream::binary);
    if(file.fail())
    {
        ErrorBox msg("Could not read the file.");
        return;
    }
    clearAllEntries();
    string line;
    while( getline(file, line) )
    {
        if(!line.empty() && line[0] != '>')
        {
            line = stripWhiteSpace(line);
            addNewSequence(line);
        }
    }
}

void HighlightDisplay::removeEntry(SequenceEntry* entry)
{
    entry->label->hide();
    formLayout->removeWidget(entry->label);
    entry->lineEdit->hide();
    formLayout->removeWidget(entry->lineEdit);
    entry->colorBox->hide();
    formLayout->removeWidget(entry->colorBox);
    entry->removeButton->hide();
    formLayout->removeWidget(entry->removeButton);

    seqLines.erase( std::find(seqLines.begin(), seqLines.end(), entry) );

    upToDate = false;
}

void HighlightDisplay::clearAllEntries()
{
    int nEntries = seqLines.size();
    for(int i = nEntries-1; i >= 0; --i)
        removeEntry(seqLines[i]);
    invalidate();
}

void HighlightDisplay::setPercentSimilarity(int percentile)
{
    double newVal = (float)percentile / 100.0;
    if( percentage_match != newVal )
    {
        percentage_match = newVal;
        invalidate();
    }
}
