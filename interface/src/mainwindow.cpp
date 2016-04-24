#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <qmessagebox.h>
#include <sstream>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_videoFilename(),
    m_analyser(),
    m_acd(),
    m_ahd(),
    m_db(nullptr)
{
    const QString dbName="game.db";

    QFileInfo check_file(dbName);
    if(!check_file.exists() || !check_file.isFile())
    {
        try
        {
            Database::create(dbName);
        }
        catch(std::runtime_error& e)
        {
            std::ostringstream oss;
            oss << "Failed to create database : " << e.what();
            QMessageBox::critical(this, "Database error", QString::fromStdString(oss.str()));
        }
    }

    try
    {
        m_db = new Database("game.db");
    }
    catch(std::runtime_error& e)
    {
        std::ostringstream oss;
        oss << "Failed to open database : " << e.what();
        QMessageBox::critical(this, "Database error", QString::fromStdString(oss.str()));
    }

    ui->setupUi(this);

    ui->widget->hide();
    ui->horizontalWidget_answer->hide();
    ui->horizontalWidget_submit->hide();

    VLCPlayer::init(&m_analyser);
    m_analyser.addAnalyser(&m_acd);
    m_analyser.addAnalyser(&m_ahd);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionLoad_triggered()
{
    m_videoFilename=QFileDialog::getOpenFileName(this, tr("Open Video"),
                                                 "/home",
                                                 tr("Videos (*.mp4 *.avi *.ogv)"));
    ui->lineEdit->setText(m_videoFilename);
}

void MainWindow::on_pushButton_clicked()
{
    on_actionLoad_triggered();
}

float MainWindow::hudMaskDistanceCalculation(const QImage *img1, const QImage *img2) const
{
    Q_ASSERT(img1!=NULL && img2!=NULL
            && img1->format()==QImage::Format_Grayscale8
            && img2->format()==QImage::Format_Grayscale8);

    int distance=0;
    quint8 *img1Ptr;
    quint8 *img2Ptr;
    int wI1, hI1, wI2, hI2;

    if(img1->width() < img2->width())
    {
        img1Ptr=(quint8 *) img2->bits();
        img2Ptr=(quint8 *) img1->bits();
        wI1= img2->width();
        hI1=img2->height();
        wI2= img1->width();
        hI2= img1->height();
    }
    else
    {
        img1Ptr=(quint8 *) img1->bits();
        img2Ptr=(quint8 *) img2->bits();
        wI1= img1->width();
        hI1=img1->height();
        wI2= img2->width();
        hI2= img2->height();
    }

    for (int x=0; x<wI1; ++x)
    {
        int xImg2=toIntCoordinate(fromIntCoordinate(x, wI1), wI2);
        for (int y=0; y<hI1; ++y)
        {
            int yImg2=toIntCoordinate(fromIntCoordinate(y, hI1), hI2);
            distance+=(img2Ptr[toPtrLocation(xImg2, yImg2, wI2)]-img1Ptr[toPtrLocation(x, y, wI1)]);
        }
    }
    return (float)distance/(wI1*hI1);
}

void MainWindow::on_pushButton_4_clicked() //Yes
{
    //We're happy
    ui->horizontalWidget_answer->hide();
    ui->widget->hide();

    eraseResults();
}

void MainWindow::on_pushButton_5_clicked() //No
{
    //We're sad and we ask for knowledge
    ui->horizontalWidget_answer->hide();
    ui->horizontalWidget_submit->show();

    eraseResults(false);
}

void MainWindow::on_pushButton_2_clicked() //Don't know
{
    //We're happy too
    on_pushButton_4_clicked();
}

void MainWindow::on_pushButton_3_clicked() //Results
{
    if(!m_videoFilename.isEmpty())
    {
        VLCPlayer::loadFile(m_videoFilename);
        VLCPlayer::release();

        m_analyser.produceOutputs();
        if(m_ahd.getImg()!=NULL)
        {
            ui->label_2->setPixmap(QPixmap::fromImage(*(m_ahd.getImg())));
            m_ahd.getImg()->save("output.png");
        }

        findGame();

        ui->widget->show();
        ui->horizontalWidget_answer->show();

    }
}

void MainWindow::on_pushButton_6_clicked() //Submit
{
    QImage emptyImage;
    QString *editor=new QString(ui->lineEdit_editor->text());
    QString *description=new QString(ui->lineEdit_desc->text());
    int *year=new int(ui->spinBox_year->value());
    Game newGame(ui->lineEdit_game->text(), *m_ahd.getImg(), editor, description, emptyImage, year);
    try{
        m_db->insert_game(newGame);
        ui->horizontalWidget_submit->hide();
        ui->widget->hide();
        eraseResults(true);
    }
    catch(std::runtime_error& e){
        std::ostringstream oss;
        oss << "Failed to submit : " << e.what();
        QMessageBox::critical(this, "Database error", QString::fromStdString(oss.str()));
    }
}

void MainWindow::eraseResults(bool readOnly)
{
    ui->lineEdit_game->setText(QString(""));
    ui->lineEdit_editor->setText(QString(""));
    ui->spinBox_year->setValue(QDate::currentDate().year());
    ui->lineEdit_desc->setText(QString(""));

    ui->lineEdit_game->setReadOnly(readOnly);
    ui->lineEdit_editor->setReadOnly(readOnly);
    ui->spinBox_year->setReadOnly(readOnly);
    ui->lineEdit_desc->setReadOnly(readOnly);
}

void MainWindow::findGame()
{
    float minDist=std::numeric_limits<float>::max();
    std::vector<QString> games=m_db->games();

    if(games.size()==0)
        return;

    Game fetchGame;
    Game bestGame;

    for(QString gameString: games)
    {
        fetchGame=m_db->game(gameString);
        float currentDist=hudMaskDistanceCalculation(m_ahd.getImg(), &(fetchGame.analysis()));
        if(minDist>currentDist)
        {
            minDist=currentDist;
            bestGame=fetchGame;
        }
    }

    ui->lineEdit_game->setText(bestGame.name());
    ui->lineEdit_editor->setText(bestGame.editor()!=NULL ? *bestGame.editor() : QString(""));
    ui->spinBox_year->setValue(bestGame.year()!=NULL ? *bestGame.year() : QDate::currentDate().year());
    ui->lineEdit_desc->setText(bestGame.description()!=NULL ? *bestGame.description() : QString(""));
}
