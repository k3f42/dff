#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

static const size_t kMaxDir=32;

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  connect(ui->addFolder, &QPushButton::clicked, this, &MainWindow::addInputFolder);
  connect(ui->removeFolder, &QPushButton::clicked, this, &MainWindow::removeFolder);
  connect(ui->find, &QPushButton::clicked, this, &MainWindow::findDuplicate);
}

MainWindow::~MainWindow()
{
  delete ui;
}


void MainWindow::addInputFolder() {
  QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory"),"", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (!dir.isEmpty()) {
    if (ui->folders->findItems(dir,Qt::MatchExactly).isEmpty())
     ui->folders->addItem(dir);
    else
      QMessageBox::warning(this,"Duplicate","Folder already in the list");
  }
}

void MainWindow::removeFolder() {
 qDeleteAll(ui->folders->selectedItems());
}


void MainWindow::findDuplicate() {
  db.clear();
  // find all folders name
  int nb=ui->folders->count();
  QProgressDialog progressDialog(this);
  progressDialog.setRange(0, nb);
  progressDialog.setWindowTitle(tr("Scan input folders"));
  bool ok=true;
  for(int i=0;i<nb;++i) {
    if (progressDialog.wasCanceled()) {
      QMessageBox::critical(this,"Cancel","Process cancelled");
      db.clear();
      return;
    }
    progressDialog.setValue(i);
    if (!addFolderDb(ui->folders->item(i)->text())) {
      QMessageBox::critical(this,"Limits","Maximum number of folders exceeded");
      db.clear();
      return;
    }
  }


  // look for duplicate
  nb=db.size();
  progressDialog.reset();
  progressDialog.setRange(0, nb);
  progressDialog.setWindowTitle(tr("Scan for duplicate folders"));
  for(int i=0;i<nb;++i) {
     progressDialog.setValue(i);
     if (progressDialog.wasCanceled()) {
       QMessageBox::critical(this,"Cancel","Process cancelled");
       db.clear();
       return;
     }
  }

  // populate table
  ui->results->setRowCount(db.size());
  int cpt=0;
  for(const auto &e : db) {
    QTableWidgetItem *newItem = new QTableWidgetItem(db.back().name);
    ui->results->setItem(cpt,0, newItem);
    newItem = new QTableWidgetItem(QString::number(db.back().nb_elts));
    ui->results->setItem(cpt,2, newItem);
    cpt++;
  }
  ui->results->resizeColumnsToContents();
}

bool MainWindow::addFolderDb(QString s) {
  if (db.size()>kMaxDir) {
    return false;
  }
  QDir dir(s);
  QString name=dir.canonicalPath();
  if (std::find_if(db.begin(),db.end(),[&](const Folder &f) { return f.name==name;})!=db.end()) return true;

  QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot);
  db.push_back(Folder());
  db.back().name=name;
  db.back().nb_elts=list.size();


  bool ok=true;
  for (int i = 0; i < list.size(); ++i) {
    QFileInfo fileInfo = list.at(i);
    if (fileInfo.isDir()) {
      QMessageBox::critical(this,"info",fileInfo.filePath());
      if (fileInfo.filePath()=="..") continue;
      // std::cout << qPrintable(QString("%1 %2").arg(fileInfo.size(), 10)
      //                        .arg(fileInfo.fileName()));
      ok&=addFolderDb(fileInfo.canonicalFilePath());
    }
  }
  return ok;
}
