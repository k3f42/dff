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
     if (!dirs_entry[i].second) { // not done yet
         Duplicates v;
         v.push_back(dirs_entry[i].first);
         dirs_entry[i].second=true;
         for(size_t j=i+1;j<dirs_entry.size();++j) {
             if (equal(*dirs_entry[i].first,*dirs_entry[j].first)) {
                 v.push_back(dirs_entry[j].first);
                 dirs_entry[j].second=true;
             }
         }
         dirs_duplicate.push_back(std::move(v));
         // if (!v.size()>1) dirs_duplicate.push_back(std::move(v));
     }
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
      newItem = new QTableWidgetItem(QString::number(first.id.crc));
      ui->results->setItem(cpt,4, newItem);
      cpt++;
  }
  ui->results->resizeColumnsToContents();

  // populate info
  ui->duplicate_nb->setText(QString::number(dirs_duplicate.size()));
  size_t size=0;
  for(const auto &e: dirs_duplicate) {
    size+=(e.size()-1)*e.front()->id.size;
  }
  ui->duplicate_size->setText(QString::number(size));
  ui->found_nb->setText(QString::number(dirs_entry.size()));
  size=0;
  for(const auto &e: dirs_entry) {
      size+=e.first->id.size;
  }
  ui->found_size->setText(QString::number(size));
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
  dirs_entry.push_back(std::make_pair(&cur,false));
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
  cur.id.crc=crc64(cur.id.name);
  count++;
  return true;
}


bool MainWindow::equal(const Element &d0,const Element &d1) const {
  if (ui->ignore_empty->isChecked()&&d0.id.size==0) return false;
  if (ui->same_name->isChecked()) {
      if (QFileInfo{d0.id.name}.fileName()!=QFileInfo{d1.id.name}.fileName()) return false;
    }
  if (ui->same_size->isChecked()) {
      if (d0.id.size!=d1.id.size) return false;
  }
  if (ui->same_crc->isChecked()) {
      if (d0.id.crc!=d1.id.crc) return false;
  }
  {
      if (d0.content.size()!=d1.content.size()) return false;
  }

  return true;
}


