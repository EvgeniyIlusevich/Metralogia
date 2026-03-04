// halsteadmetrics.cpp — обновлённый (учитывает import/package и квалифицированные имена)
#include "halsteadmetrics.h"
#include <QDebug>
#include <QRegularExpression>
#include <QVector>

/*
  Исправленный файл halsteadmetrics.cpp
  - исключаем части import/package из подсчёта операндов;
  - квалифицированные имена (a.b.c) считаются единым операндом;
  - пометка занятых диапазонов для предотвращения двойного учёта.
*/

QSet<QString> HalsteadMetrics::keywords = {
    "as",		 "as?",	 "break", "class",	"continue",	 "do",
    "else",		 "for",	 "fun",	  "if",		"in",		 "!in",
    "interface", "is",	 "!is",	  "object", "package",	 "return",
    "super",	 "this", "throw", "try",	"typealias", "val",
    "var",		 "when", "while", "main",	"import"};

QSet<QString> HalsteadMetrics::booleanLiterals = {"true", "false", "null"};

namespace {
// Проверка пересечения диапазона [start, start+len) с уже имеющимися


static bool overlapsAny(const QVector<QPair<int, int>>& ranges, int start,
                        int len) {
    if (len <= 0)
        return true;
    int end = start + len;
    for (const auto& r : ranges) {
        int rs = r.first;
        int re = r.first + r.second;
        if (start < re && rs < end)
            return true;
    }
    return false;
}


static void addRange(QVector<QPair<int, int>>& ranges, int start, int len) {
    if (len <= 0)
        return;
    ranges.append(qMakePair(start, len));
}

}  // namespace

void HalsteadMetrics::analyze(const QString& code,
                              QMap<QString, int>& operators,
                              QMap<QString, int>& operands) {
    operators.clear();
    operands.clear();

    QString cleanedCode = removeComments(code);

    // список диапазонов уже найденных операторов — чтобы избежать перекрытий
    QVector<QPair<int, int>> occupiedOpRanges;

    // ---- Операторы (Kotlin-подобные) ----
    QList<QRegularExpression> operatorRegexes;

    // 1. Комбинированный паттерн для большинства операторов
    operatorRegexes << QRegularExpression(
        "===|!==|"
        "\\+\\+|--|\\+=|-=|\\*=|/|%=|&=|\\|=|\\^=|"
        "==|!=|<=|>=|<|(?<!-)>|=|\\+|-(?!>)|/|\\*|%|&&|\\|\\||!");

    // 2. !in / in (длиннее сначала)
    operatorRegexes << QRegularExpression("\\b!in\\b|\\bin\\b");

    // 3. !is / is
    operatorRegexes << QRegularExpression("\\b!is\\b|\\bis\\b");

    // 4. Элвис
    operatorRegexes << QRegularExpression("\\?:");

    // 5. Управление потоком: return, continue, break
    operatorRegexes << QRegularExpression(
        "\\breturn\\b|\\bcontinue\\b|\\bbreak\\b");

    // 6. Условные: if, when
    operatorRegexes << QRegularExpression("\\bif\\b|\\bwhen\\b");

    // 7. Циклы: do, while, for, repeat
    operatorRegexes << QRegularExpression(
        "\\bdo\\b|\\bwhile\\b|\\bfor\\b|\\brepeat\\b");

    // 8. Разделитель ;
    operatorRegexes << QRegularExpression(";");

    // 9. Диапазоны '..' и '..<'
    operatorRegexes << QRegularExpression("\\.\\.<|\\.\\.");

    // 10. Индексный доступ (квадратные скобки)
    operatorRegexes << QRegularExpression("\\[");
    operatorRegexes << QRegularExpression("\\]");

    // 11. Вызов функции: идентификатор, за которым следуют круглые скобки с содержимым (упрощённо,
    //     без поддержки вложенных скобок внутри аргументов)
    operatorRegexes << QRegularExpression("\\b[A-Za-z_]\\w*\\s*\\([^()]*\\)");

    // 12. Фигурные скобки отдельно
    operatorRegexes << QRegularExpression("\\{");
    operatorRegexes << QRegularExpression("\\}");

    // 13. Круглые скобки
    operatorRegexes << QRegularExpression("\\(");
    operatorRegexes << QRegularExpression("\\)");

    // 14. Битовые/слово-операторы
    operatorRegexes << QRegularExpression(
        "\\band\\b|\\bor\\b|\\bxor\\b|\\binv\\b|\\bshl\\b|\\bshr\\b|"
        "\\bushr\\b");

    // 15. Ключевые слова объявления (fun, val, var) – как операторы
    operatorRegexes << QRegularExpression("\\bfun\\b|\\bval\\b|\\bvar\\b");

    // Проходим по всем паттернам и собираем операторы, пропуская пересекающиеся диапазоны


    for (const QRegularExpression& re : operatorRegexes) {
        QRegularExpressionMatchIterator it = re.globalMatch(cleanedCode);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int start = match.capturedStart();
            int len = match.capturedLength();
            if (len <= 0)
                continue;
            if (overlapsAny(occupiedOpRanges, start, len))
                continue;

            QString opText = match.captured();

            // Особая обработка вызова функции: считаем оператором "call:<name>"
            QRegularExpression funcNameRegex("^\\b([A-Za-z_]\\w*)\\s*\\(");
            QRegularExpressionMatch nameMatch = funcNameRegex.match(opText);
            if (nameMatch.hasMatch()) {
                QString funcName = nameMatch.captured(1);
                if (!keywords.contains(funcName)) {
                    QString callToken = QStringLiteral("call:") + funcName;
                    operators[callToken] = operators.value(callToken, 0) + 1;
                    addRange(occupiedOpRanges, start, len);
                    continue;
                }
                // иначе (если имя — ключевое слово) fall-through
            }

            operators[opText] = operators.value(opText, 0) + 1;
            addRange(occupiedOpRanges, start, len);
        }
    }

    // --- Обозначим декларации import/package как занятые диапазоны,
    // чтобы их части (kotlin, math и т.п.) не считались операндами ---
    QRegularExpression importPkgRegex(
        "\\b(?:import|package)\\s+[A-Za-z_]\\w*(?:\\.[A-Za-z_]\\w*)*(?:\\s*;)"
        "?");
    QRegularExpressionMatchIterator ipIt =
        importPkgRegex.globalMatch(cleanedCode);
    while (ipIt.hasNext()) {
        QRegularExpressionMatch match = ipIt.next();
        int start = match.capturedStart();
        int len = match.capturedLength();
        if (len <= 0)
            continue;
        addRange(occupiedOpRanges, start, len);
    }

    // ---- Операнды ----
    QVector<QPair<int, int>>
        occupiedOperandRanges;	// диапазоны уже найденных операндов (литералы и т.д.)

    // 1. Строковые литералы
    QRegularExpression stringRegex("\"([^\"\\\\]*(\\\\.[^\"\\\\]*)*)\"");
    QRegularExpressionMatchIterator strIt =
        stringRegex.globalMatch(cleanedCode);
    while (strIt.hasNext()) {
        QRegularExpressionMatch match = strIt.next();
        int start = match.capturedStart();
        int len = match.capturedLength();
        if (len <= 0)
            continue;
        if (overlapsAny(occupiedOpRanges, start, len))
            continue;
        QString lit = match.captured();
        operands[lit] = operands.value(lit, 0) + 1;
        addRange(occupiedOperandRanges, start, len);
    }

    // 2. Символьные литералы
    QRegularExpression charRegex("'([^'\\\\]*(\\\\.[^'\\\\]*)*)'");
    QRegularExpressionMatchIterator charIt = charRegex.globalMatch(cleanedCode);
    while (charIt.hasNext()) {
        QRegularExpressionMatch match = charIt.next();
        int start = match.capturedStart();
        int len = match.capturedLength();
        if (len <= 0)
            continue;
        if (overlapsAny(occupiedOpRanges, start, len))
            continue;
        QString lit = match.captured();
        operands[lit] = operands.value(lit, 0) + 1;
        addRange(occupiedOperandRanges, start, len);
    }

    // 3. Числовые литералы (целые, с плавающей точкой, экспонентой)
    QRegularExpression numRegex("\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b");
    QRegularExpressionMatchIterator numIt = numRegex.globalMatch(cleanedCode);
    while (numIt.hasNext()) {
        QRegularExpressionMatch match = numIt.next();
        int start = match.capturedStart();
        int len = match.capturedLength();
        if (len <= 0)
            continue;
        if (overlapsAny(occupiedOpRanges, start, len))
            continue;
        QString lit = match.captured();
        operands[lit] = operands.value(lit, 0) + 1;
        addRange(occupiedOperandRanges, start, len);
    }

    // 4. Логические литералы и null


    for (const QString& lit : booleanLiterals) {
        QRegularExpression litRegex("\\b" + QRegularExpression::escape(lit) +
                                    "\\b");
        QRegularExpressionMatchIterator litIt =
            litRegex.globalMatch(cleanedCode);
        while (litIt.hasNext()) {
            QRegularExpressionMatch match = litIt.next();
            int start = match.capturedStart();
            int len = match.capturedLength();
            if (len <= 0)
                continue;
            if (overlapsAny(occupiedOpRanges, start, len) ||
                overlapsAny(occupiedOperandRanges, start, len))
                continue;
            operands[lit] = operands.value(lit, 0) + 1;
            addRange(occupiedOperandRanges, start, len);
        }
    }

    // 5. Квалифицированные имена (a.b.c) — считаем их единым операндом
    QRegularExpression qualifiedRegex(
        "\\b[A-Za-z_]\\w*(?:\\.[A-Za-z_]\\w*)+\\b");
    QRegularExpressionMatchIterator qIt =
        qualifiedRegex.globalMatch(cleanedCode);
    while (qIt.hasNext()) {
        QRegularExpressionMatch match = qIt.next();
        int start = match.capturedStart();
        int len = match.capturedLength();
        if (len <= 0)
            continue;
        if (overlapsAny(occupiedOpRanges, start, len) ||
            overlapsAny(occupiedOperandRanges, start, len))
            continue;
        QString qname = match.captured();
        // Не учитываем ключевые слова (на всякий)
        if (keywords.contains(qname))
            continue;
        operands[qname] = operands.value(qname, 0) + 1;
        addRange(occupiedOperandRanges, start, len);
    }

    // 6. Идентификаторы (слова) — исключая ключевые слова и уже занятые диапазоны
    QRegularExpression idRegex("\\b([A-Za-z_]\\w*)\\b");
    QRegularExpressionMatchIterator idIt = idRegex.globalMatch(cleanedCode);
    while (idIt.hasNext()) {
        QRegularExpressionMatch match = idIt.next();
        int start = match.capturedStart(1);
        int len = match.capturedLength(1);
        if (len <= 0)
            continue;
        if (overlapsAny(occupiedOpRanges, start, len) ||
            overlapsAny(occupiedOperandRanges, start, len))
            continue;

        QString ident = match.captured(1);
        if (keywords.contains(ident) || booleanLiterals.contains(ident))
            continue;

        operands[ident] = operands.value(ident, 0) + 1;
        addRange(occupiedOperandRanges, start, len);
    }
}

QString HalsteadMetrics::removeComments(const QString& code) {
    QString result = code;
    // Однострочные комментарии
    QRegularExpression singleLineComment("//[^\\n]*");
    result.remove(singleLineComment);
    // Многострочные комментарии
    QRegularExpression multiLineComment(
        "/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption);
    result.remove(multiLineComment);
    return result;
}
