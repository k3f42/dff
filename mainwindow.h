#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <vector>
#include <QMainWindow>

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
  struct Element {
    QString name;
    //Id id;
  };

  struct Folder {
    QString name;
    size_t nb_elts;
    std::vector<Element> cache;
  };

  Ui::MainWindow *ui;
  std::vector<Folder> db;
  bool addFolderDb(QString s);


private slots:
  void addInputFolder();
  void removeFolder();
  void findDuplicate();
};

#endif // MAINWINDOW_H
