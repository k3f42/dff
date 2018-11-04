#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <vector>
#include <QMainWindow>
#include <memory>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private:
  struct Id {
    QString name;
    size_t size;
    uint64_t crc;
    // date
  };

  struct Element {
    Id id;
    std::vector<std::unique_ptr<Element>> content; // file: content is empty
  };

  Ui::MainWindow *ui;
  std::vector<std::unique_ptr<Element>> db; // roots of the tree
  size_t count;

  using DirEntry=std::pair<Element *,bool>; // bool=done
  std::vector<DirEntry> dirs_entry; // dirs only
  using Duplicates=std::vector<Element *>;
  std::vector<Duplicates> dirs_duplicate;

  bool addElementDb(std::vector<std::unique_ptr<Element> > &dbi, QString s);
  bool addFileDb(std::vector<std::unique_ptr<Element> > &dbi, QString s);
  bool equal(const Element &d0,const Element &d1) const;

private slots:
  void addInputFolder();
  void removeFolder();
  void findDuplicate();
};

#endif // MAINWINDOW_H
