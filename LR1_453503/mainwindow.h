#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

QT_BEGIN_NAMESPACE
class QPushButton;
class QLabel;
class QTableWidget;
class QTextEdit;
class QGroupBox;
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

   private slots:
    void onOpenFile();
    void analyzeFile(const QString& fileName);

   private:
    void setupUi();
    void updateMetricsDisplay();
    void updateTables();

    QPushButton* m_openButton;
    QLabel* m_fileLabel;
    QTableWidget* m_operatorTable;
    QTableWidget* m_operandTable;
    QLabel* m_metricsLabels[11];
    QTextEdit* m_logText;

    QMap<QString, int> m_operators;
    QMap<QString, int> m_operands;
    int m_totalOperators;
    int m_totalOperands;
    int m_uniqueOperators;
    int m_uniqueOperands;
};

#endif	// MAINWINDOW_H
