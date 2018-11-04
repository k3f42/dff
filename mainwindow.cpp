#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <iostream>
#include "crc.h"

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
  dirs_entry.clear();
  count=0;
  // find all folders name
  size_t nb=ui->folders->count();
  QProgressDialog progressDialog(this);
  progressDialog.setRange(0, nb);
  progressDialog.setWindowTitle(tr("Scan input folders"));

  for(size_t i=0;i<nb;++i) {
    if (progressDialog.wasCanceled()) {
      QMessageBox::critical(this,"Cancel","Process cancelled");
      db.clear();
      return;
    }
    progressDialog.setValue(i);
    if (!addElementDb(db,ui->folders->item(i)->text())) {
      QMessageBox::critical(this,"Limits","Maximum number of folders exceeded");
      db.clear();
      return;
    }
  }

  // look for duplicate
  dirs_duplicate.clear();
  nb=dirs_entry.size();
  progressDialog.reset();
  progressDialog.setRange(0, nb);
  progressDialog.setWindowTitle(tr("Scan for duplicate folders"));
  for(size_t i=0;i<nb;++i) {
     progressDialog.setValue(i);
     if (progressDialog.wasCanceled()) {
       QMessageBox::critical(this,"Cancel","Process cancelled");
       db.clear();
       return;
     }
     // todo
     dirs_duplicate.push_back(std::vector<Element *>({dirs_entry[i]}));
  }

  // populate table
  ui->results->setRowCount(dirs_duplicate.size());
  int cpt=0;
  for(const auto &e : dirs_duplicate) {
      const Element &first=*e.front();
      QTableWidgetItem *newItem = new QTableWidgetItem(first.id.name);
      ui->results->setItem(cpt,0, newItem);
      newItem = new QTableWidgetItem(QString::number(e.size()));
      ui->results->setItem(cpt,1, newItem);
      newItem = new QTableWidgetItem(QString::number(first.id.size));
      ui->results->setItem(cpt,2, newItem);
      newItem = new QTableWidgetItem(QString::number(first.content.size()));
      ui->results->setItem(cpt,3, newItem);
      cpt++;
  }
  ui->results->resizeColumnsToContents();
}

bool MainWindow::addElementDb(std::vector<std::unique_ptr<Element>> &dbi,QString s) {
  if (count>kMaxDir) {
    return false;
  }
  QDir dir(s);
  QString name=dir.canonicalPath();
  if (std::find_if(dbi.begin(),dbi.end(),[&](const std::unique_ptr<Element> &f) { return f->id.name==name;})!=dbi.end()) return true;

  QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::Files|QDir::Dirs);
  dbi.emplace_back(new Element);
  Element &cur=*dbi.back();
  dirs_entry.push_back(&cur);
  cur.id.name=name;
  count++;

  bool ok=true;
  for (int i = 0; i < list.size(); ++i) {
    QFileInfo fileInfo = list.at(i);    
    if (fileInfo.isDir()) {
      ok&=addElementDb(cur.content,fileInfo.canonicalFilePath());
    } else {
        ok&=addFileDb(cur.content,fileInfo.canonicalFilePath());
      }
   }

  // compute size and crc
  cur.id.size=0;
  cur.id.crc=0;
  for(const auto &e: cur.content) {
      cur.id.size+=e->id.size;
      cur.id.crc=crc64(cur.id.crc,(const unsigned char *)&e->id.crc,sizeof(e->id.crc));
    }
  return ok;
}

bool MainWindow::addFileDb(std::vector<std::unique_ptr<Element>> &dbi,QString s) {
  if (count>kMaxDir) {
    return false;
  }
  QFileInfo file(s);
  dbi.emplace_back(new Element);
  Element &cur=*dbi.back();
  cur.id.name=file.canonicalFilePath();
  cur.id.size=file.size();
  cur.id.crc=0; // crc64(0,cur.id.name);
  count++;
  return true;
}
