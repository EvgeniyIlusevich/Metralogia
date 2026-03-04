#include "mainwindow.h"
#include "halsteadmetrics.h"

#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_totalOperators(0),
      m_totalOperands(0),
      m_uniqueOperators(0),
      m_uniqueOperands(0) {
    setupUi();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    setWindowTitle("Halstead Metrics for Kotlin");
    resize(900, 700);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    QHBoxLayout* topLayout = new QHBoxLayout();
    m_openButton = new QPushButton("Открыть файл...");
    m_fileLabel = new QLabel("Файл не выбран");
    m_fileLabel->setStyleSheet("QLabel { color: gray; }");
    topLayout->addWidget(m_openButton);
    topLayout->addWidget(m_fileLabel, 1);
    topLayout->addStretch();

    QHBoxLayout* centerLayout = new QHBoxLayout();

    QVBoxLayout* tablesLayout = new QVBoxLayout();

    QGroupBox* opGroup = new QGroupBox("Операторы");
    m_operatorTable = new QTableWidget(0, 2);
    m_operatorTable->setHorizontalHeaderLabels({"Оператор", "Частота"});
    m_operatorTable->horizontalHeader()->setStretchLastSection(true);
    QVBoxLayout* opLayout = new QVBoxLayout(opGroup);
    opLayout->addWidget(m_operatorTable);

    QGroupBox* operandGroup = new QGroupBox("Операнды");
    m_operandTable = new QTableWidget(0, 2);
    m_operandTable->setHorizontalHeaderLabels({"Операнд", "Частота"});
    m_operandTable->horizontalHeader()->setStretchLastSection(true);
    QVBoxLayout* operandLayout = new QVBoxLayout(operandGroup);
    operandLayout->addWidget(m_operandTable);

    tablesLayout->addWidget(opGroup);
    tablesLayout->addWidget(operandGroup);

    QGroupBox* metricsGroup = new QGroupBox("Метрики Холстеда");
    QGridLayout* metricsLayout = new QGridLayout(metricsGroup);

    const QStringList metricNames = {
        "n1 (уникальные операторы)", "n2 (уникальные операнды)",
        "N1 (всего операторов)",	 "N2 (всего операндов)",
        "n = n1+n2 (словарь)",		 "N = N1+N2 (длина)",
        "V = N * log2(n) (объём)",	 "D = (n1/2)*(N2/n2) (сложность)",
        "E = D * V (трудоёмкость)",	 "T = E/18 сек (время)",
        "B = V/3000 (ошибки)"};


    for (int i = 0; i < 11; ++i) {
        QLabel* nameLabel = new QLabel(metricNames[i] + ":");
        nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_metricsLabels[i] = new QLabel("—");
        m_metricsLabels[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
        metricsLayout->addWidget(nameLabel, i, 0);
        metricsLayout->addWidget(m_metricsLabels[i], i, 1);
    }

    centerLayout->addLayout(tablesLayout, 2);
    centerLayout->addWidget(metricsGroup, 1);

    m_logText = new QTextEdit();
    m_logText->setMaximumHeight(150);
    m_logText->setReadOnly(true);
    m_logText->setPlaceholderText("Лог анализа...");

    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(centerLayout, 1);
    mainLayout->addWidget(m_logText);

    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);
}

void MainWindow::onOpenFile() {
    QString fileName =
        QFileDialog::getOpenFileName(this, "Выберите файл Kotlin", QString(),
                                     "Kotlin files (*.kt);;All files (*.*)");
    if (fileName.isEmpty())
        return;

    m_fileLabel->setText(fileName);
    analyzeFile(fileName);
}

void MainWindow::analyzeFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл.");
        return;
    }

    QTextStream stream(&file);
    QString code = stream.readAll();
    file.close();

    m_logText->append("Файл загружен: " + fileName);
    m_logText->append("Размер: " + QString::number(code.size()) + " символов");

    HalsteadMetrics::analyze(code, m_operators, m_operands);

    m_totalOperators = 0;
    for (int cnt : m_operators)
        m_totalOperators += cnt;

    m_totalOperands = 0;
    for (int cnt : m_operands)
        m_totalOperands += cnt;

    m_uniqueOperators = m_operators.size();
    m_uniqueOperands = m_operands.size();

    updateTables();
    updateMetricsDisplay();

    m_logText->append("Анализ завершён.");
}

void MainWindow::updateTables() {
    m_operatorTable->setRowCount(m_operators.size());
    int row = 0;
    for (auto it = m_operators.constBegin(); it != m_operators.constEnd();
         ++it, ++row) {
        QTableWidgetItem* opItem = new QTableWidgetItem(it.key());
        opItem->setFlags(opItem->flags() & ~Qt::ItemIsEditable);
        m_operatorTable->setItem(row, 0, opItem);

        QTableWidgetItem* freqItem =
            new QTableWidgetItem(QString::number(it.value()));
        freqItem->setFlags(freqItem->flags() & ~Qt::ItemIsEditable);
        freqItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_operatorTable->setItem(row, 1, freqItem);
    }
    m_operatorTable->sortItems(1, Qt::DescendingOrder);

    m_operandTable->setRowCount(m_operands.size());
    row = 0;
    for (auto it = m_operands.constBegin(); it != m_operands.constEnd();
         ++it, ++row) {
        QTableWidgetItem* opItem = new QTableWidgetItem(it.key());
        opItem->setFlags(opItem->flags() & ~Qt::ItemIsEditable);
        m_operandTable->setItem(row, 0, opItem);

        QTableWidgetItem* freqItem =
            new QTableWidgetItem(QString::number(it.value()));
        freqItem->setFlags(freqItem->flags() & ~Qt::ItemIsEditable);
        freqItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_operandTable->setItem(row, 1, freqItem);
    }
    m_operandTable->sortItems(1, Qt::DescendingOrder);
}

void MainWindow::updateMetricsDisplay() {
    int n1 = m_uniqueOperators;
    int n2 = m_uniqueOperands;
    int N1 = m_totalOperators;
    int N2 = m_totalOperands;

    int vocabulary = n1 + n2;
    int length = N1 + N2;
    double volume = length * log2(vocabulary);
    double difficulty = (n2 == 0) ? 0 : (n1 / 2.0) * (N2 / (double)n2);
    double effort = difficulty * volume;
    double time = effort / 18.0;
    double bugs = volume / 3000.0;

    m_metricsLabels[0]->setText(QString::number(n1));
    m_metricsLabels[1]->setText(QString::number(n2));
    m_metricsLabels[2]->setText(QString::number(N1));
    m_metricsLabels[3]->setText(QString::number(N2));
    m_metricsLabels[4]->setText(QString::number(vocabulary));
    m_metricsLabels[5]->setText(QString::number(length));
    m_metricsLabels[6]->setText(QString::number(volume, 'f', 2));
    m_metricsLabels[7]->setText(QString::number(difficulty, 'f', 2));
    m_metricsLabels[8]->setText(QString::number(effort, 'f', 2));
    m_metricsLabels[9]->setText(QString::number(time, 'f', 2) + " сек (" +
                                QString::number(time / 60.0, 'f', 2) + " мин)");
    m_metricsLabels[10]->setText(QString::number(bugs, 'f', 3));
}
