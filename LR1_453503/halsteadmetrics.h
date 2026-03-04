#ifndef HALSTEADMETRICS_H
#define HALSTEADMETRICS_H

#include <QMap>
#include <QSet>
#include <QString>

class HalsteadMetrics {
   public:
    static void analyze(const QString& code, QMap<QString, int>& operators,
                        QMap<QString, int>& operands);

   private:
    static QString removeComments(const QString& code);
    static QSet<QString> keywords;
};

#endif	// HALSTEADMETRICS_H
