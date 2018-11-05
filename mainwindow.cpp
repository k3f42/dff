#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <iostream>
#include "crc.h"
#include <QDebug>

static const size_t kMaxDir=32*1024*1024;
static const size_t kRefreshGuiCount=128;

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  connect(ui->addFolder, &QPushButton::clicked, this, &MainWindow::addInputFolder);
  connect(ui->removeFolder, &QPushButton::clicked, this, &MainWindow::removeFolder);
  connect(ui->find, &QPushButton::clicked, this, &MainWindow::findDuplicate);
  connect(ui->results, &QTableWidget::cellDoubleClicked, this, &MainWindow::showResult);

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
  max_folder_depth=0;

  if (!progressDialog) {
    progressDialog=new QProgressDialog("Scan input folders","Cancel",0,0,this);
    progressDialog->setMinimumDuration(0);
    progressDialog->setWindowModality(Qt::WindowModal);
    QApplication::processEvents();
  }
  // find all folders name
  {
    progressDialog->setLabelText("Scan input folders");
    progressDialog->setRange(0,0);
    progressDialog->show();
    size_t nb=ui->folders->count();
    for(size_t i=0;i<nb;++i) {

      if (!addFolderToDb(db,ui->folders->item(i)->text(),0)) {
        QMessageBox::critical(this,"Limits","Maximum number of folders exceeded");
        db.clear();
        progressDialog->close();
        return;
      }
    }
  }
  // populate dirs_entry by depth order
  {
    std::vector<std::vector<Element *>> v{(size_t)max_folder_depth+1};
    for(const auto &e: db) addDirsEntry(*e,v);
    dirs_entry.clear();
    for(const auto &e: v)
      for(const auto &ee: e) {
          dirs_entry.push_back(std::make_pair(ee,false));
        }
  }

  // look for duplicate
  {
    progressDialog->setLabelText("Search for duplicate");
    size_t nb=dirs_entry.size();;
    progressDialog->setRange(0,nb);
    progressDialog->show();
    dirs_duplicate.clear();
    for(size_t i=0;i<nb;++i) {
      progressDialog->setValue(i);
      if (progressDialog->wasCanceled()) {
        QMessageBox::critical(this,"Cancel","Process cancelled");
        db.clear();
        progressDialog->close();
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
        // dirs_duplicate.push_back(std::move(v));
        if (v.size()>1) dirs_duplicate.push_back(std::move(v));
      }
    }
  }
  // populate table
  ui->results->clearContents();
  ui->results->setRowCount(dirs_duplicate.size());
  int cpt=0;
  for(const auto &e : dirs_duplicate) {
      const Element &first=*e.front();
      QTableWidgetItem *newItem = new QTableWidgetItem(first.id.name);
      newItem->setToolTip(first.id.name);
      ui->results->setItem(cpt,0, newItem);
      newItem = new QTableWidgetItem(QString::number(e.size()));
      ui->results->setItem(cpt,1, newItem);
      newItem = new QTableWidgetItem(QString::number(first.id.size/1024/1024));
      ui->results->setItem(cpt,2, newItem);
      newItem = new QTableWidgetItem(QString::number(first.content.size()));
      ui->results->setItem(cpt,3, newItem);
     // newItem = new QTableWidgetItem(QString::number(first.id.crc,16));
    //  ui->results->setItem(cpt,4, newItem);
      cpt++;
  }
  ui->results->resizeColumnsToContents();

  // populate info
  ui->duplicate_nb->setText(QString::number(dirs_duplicate.size()));
  size_t size=0;
  for(const auto &e: dirs_duplicate) {
    size+=(e.size()-1)*e.front()->id.size;
  }
  ui->duplicate_size->setText(QString::number(size/1024/1024));
  ui->found_nb->setText(QString::number(dirs_entry.size()));
  size=0;
  for(const auto &e: dirs_entry) {
      size+=e.first->id.size;
  }
  ui->found_size->setText(QString::number(size/1024/1024));
  progressDialog->close();
}

void MainWindow::addDirsEntry(Element &e,std::vector<std::vector<Element *>> &v) const {
  if (!e.is_dir) return;
  if (e.depth>=(int)v.size()) {
      QMessageBox::critical(0,"Logical error","depth too large");
      QApplication::quit();
    }
  v[e.depth].push_back(&e);
  for(const auto &ee: e.content) {
      addDirsEntry(*ee,v);
  }
}


bool MainWindow::addFolderToDb(std::vector<std::unique_ptr<Element>> &dbi,QString s,int depth) {
  if (count>kMaxDir) {
    return false;
  }
  QDir dir(s);
  QString name=dir.canonicalPath();

  if (std::find_if(dbi.begin(),dbi.end(),[&](const std::unique_ptr<Element> &f) { return f->id.name==name;})!=dbi.end()) return true;

  QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::Files|QDir::Dirs|QDir::NoSymLinks);
  dbi.emplace_back(new Element);
  Element &cur=*dbi.back();
  cur.id.name=name;
  cur.is_dir=true;
  cur.depth=depth;
  if (max_folder_depth<depth) max_folder_depth=depth;
  count++;

  if ((count%kRefreshGuiCount)==0) {
    if (progressDialog->wasCanceled()) {
      QMessageBox::critical(this,"Cancel","Process cancelled");
      db.clear();
      return false;
    }
    progressDialog->setValue(0);
    QApplication::processEvents( ); // QEventLoop::ExcludeUserInputEvents);
  }

  bool ok=true;
  for (int i = 0; i < list.size(); ++i) {
    QFileInfo fileInfo = list.at(i);
    if (fileInfo.isDir()) {
      ok&=addFolderToDb(cur.content,fileInfo.canonicalFilePath(),depth+1);
    } else {
      ok&=addFileToDb(cur.content,fileInfo.canonicalFilePath(),depth);
    }
  }

  // compute size and crc
  cur.id.size=0;
  cur.id.crc=0;
  for(const auto &e: cur.content) {
    cur.id.size+=e->id.size;
  }
  return ok;
}

bool MainWindow::addFileToDb(std::vector<std::unique_ptr<Element>> &dbi,QString s,int depth) {
  if (count>kMaxDir) {
    return false;
  }
  QFileInfo file(s);
  dbi.emplace_back(new Element);
  Element &cur=*dbi.back();
  cur.id.name=file.canonicalFilePath().toUtf8();
  cur.id.size=file.size();
  cur.id.crc_done=false;
  cur.is_dir=false;
  cur.depth=depth;
  count++;
  if ((count%kRefreshGuiCount)==0) {
    if (progressDialog->wasCanceled()) {
      QMessageBox::critical(this,"Cancel","Process cancelled");
      db.clear();
      return false;
    }
    progressDialog->setValue(0);
    QApplication::processEvents( ); // QEventLoop::ExcludeUserInputEvents);
  }

  return true;
}

bool MainWindow::crc64(Element &e) const {
  if (e.is_dir) {
    e.id.crc=0;
    for(auto &ee: e.content) {
      crc64(*ee);
      e.id.crc=::crc64(e.id.crc,(const unsigned char *)&ee->id.crc,sizeof(ee->id.crc));
    }
  } else {
    bool ok;
    e.id.crc=::crc64(e.id.name,ok);
    if (!ok) return false;
  }

  e.id.crc_done=true;
  return true;
}

bool MainWindow::equal( Element &d0, Element &d1) const {
  if (ui->ignore_empty->isChecked()&&d0.id.size==0) return false;
  if (ui->same_name->isChecked()) {
      if (QFileInfo{d0.id.name}.fileName()!=QFileInfo{d1.id.name}.fileName()) return false;
    }
  if (ui->same_size->isChecked()) {
      if (d0.id.size!=d1.id.size) return false;
  }
  {
      if (d0.content.size()!=d1.content.size()) return false;
  }
  if (ui->same_crc->isChecked()) {
    bool ok=true;
    if (!d0.id.crc_done) ok&=crc64(d0);
    if (!d1.id.crc_done) ok&=crc64(d1);
    if (d0.id.crc!=d1.id.crc) return false;
    return ok;
  }
  return true;
}

void MainWindow::showResult(int r,int /*c*/) {
  QMessageBox msgBox;
  msgBox.setText("Duplicates found");
  QString s;
  for(const auto &e: dirs_duplicate[r]) {
    s+=e->id.name+"\n";
  }
  msgBox.setInformativeText(s); // setDetailedText
  msgBox.exec();
}
