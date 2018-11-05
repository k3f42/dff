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
    bool crc_done;
  };

  struct Element {
    Id id;
    std::vector<std::unique_ptr<Element>> content;
    unsigned short depth;
    bool is_dir;
  };

  Ui::MainWindow *ui;
  std::vector<std::unique_ptr<Element>> db; // roots of the tree
  size_t count;
  int max_folder_depth;

  using DirEntry=std::pair<Element *,bool>; // bool=done
  std::vector<DirEntry> dirs_entry; // dirs only, sorted by depth (top dir first)
  using Duplicates=std::vector<Element *>;
  std::vector<Duplicates> dirs_duplicate;

  bool addFolderToDb(std::vector<std::unique_ptr<Element> > &dbi, QString s, int depth);
  bool addFileToDb(std::vector<std::unique_ptr<Element> > &dbi, QString s, int depth);
  bool equal(Element &d0,Element &d1) const;
  void addDirsEntry(Element &e, std::vector<std::vector<Element *> > &v) const;
  bool crc64(Element &e) const;

private slots:
  void addInputFolder();
  void removeFolder();
  void findDuplicate();
  void showResult(int r,int c);
};

#endif // MAINWINDOW_H
