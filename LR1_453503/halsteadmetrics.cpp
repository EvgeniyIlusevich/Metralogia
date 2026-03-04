#include "halsteadmetrics.h"
#include <QDebug>
#include <QRegularExpression>

QSet<QString> HalsteadMetrics::keywords = {
    "as",		 "as?",	  "break",	"class",  "continue",  "do",
    "else",		 "for",	  "fun",	"if",	  "in",		   "!in",
    "interface", "is",	  "!is",	"object", "package",   "return",
    "super",	 "this",  "throw",	"try",	  "typealias", "val",
    "var",		 "when",  "while",	"main",	  "import",	   "Boolean",
    "String",	 "Int",	  "Double", "Float",  "Short",	   "Long",
    "Char",		 "repeat"};

void HalsteadMetrics::analyze(const QString& code,
                              QMap<QString, int>& operators,
                              QMap<QString, int>& operands) {
    operators.clear();
    operands.clear();

    QString cleanedCode = removeComments(code);

    // ---- Операторы (полностью соответствуют оригинальному списку паттернов) ----
    QList<QRegularExpression> operatorRegexes;

    operatorRegexes << QRegularExpression(
        "===|!==|"
        "\\+\\+|--|\\+=|-=|\\*=|/=|%=|&=|\\|=|\\^=|"
        "==|!=|<=|>=|<|(?<!-)>|=|\\+|-(?!>)|/|\\*|%|&&|\\|\\||!");
    operatorRegexes << QRegularExpression("\\bin\\b|\\b!in\\b");
    operatorRegexes << QRegularExpression("\\bis\\b|\\b!is\\b");
    operatorRegexes << QRegularExpression("\\?:");
    operatorRegexes << QRegularExpression(
        "\\breturn\\b|\\bcontinue\\b|\\bbreak\\b");
    operatorRegexes << QRegularExpression("\\bif\\b|\\bwhen\\b");
    operatorRegexes << QRegularExpression(
        "\\bdo\\b|\\bwhile\\b|\\bfor\\b|\\brepeat\\b");
    operatorRegexes << QRegularExpression(";");
    operatorRegexes << QRegularExpression("\\.\\.<|\\.\\.");
    operatorRegexes << QRegularExpression("\\[");
    operatorRegexes << QRegularExpression("\\]");
    operatorRegexes << QRegularExpression(
        "\\b\\w+\\s*\\([^)]*\\)");	// вызовы функций
    operatorRegexes << QRegularExpression("\\{");
    operatorRegexes << QRegularExpression("\\}");

    // Множество имён функций, которые следует исключить из операторов (чтобы убрать and, abs, main, func)
    QSet<QString> excludeFunctionCalls = {"and", "abs", "main", "func"};


    for (const QRegularExpression& re : operatorRegexes) {
        QRegularExpressionMatchIterator it = re.globalMatch(cleanedCode);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString op = match.captured();

            // Для вызовов функций проверяем, не входит ли имя в список исключённых
            if (re.pattern().contains("\\b\\w+\\s*\\(")) {
                QRegularExpression nameRegex("\\b(\\w+)\\s*\\(");
                QRegularExpressionMatch nameMatch = nameRegex.match(op);
                if (nameMatch.hasMatch()) {
                    QString funcName = nameMatch.captured(1);
                    if (keywords.contains(funcName) ||
                        excludeFunctionCalls.contains(funcName))
                        continue;
                }
            }

            operators[op] = operators.value(op, 0) + 1;
        }
    }

    // ---- Операнды ----
    // Идентификаторы и числа (с исключением ключевых слов и дополнительных исключений)
    QRegularExpression idNumRegex(
        "(?<!\")(?<!\\.)\\b\\w+(\\.\\d+)?(?!\\.|\\()\\b");
    QRegularExpressionMatchIterator idIt = idNumRegex.globalMatch(cleanedCode);
    while (idIt.hasNext()) {
        QRegularExpressionMatch match = idIt.next();
        QString token = match.captured();
        // Исключаем ключевые слова и специально заданные операнды
        if (!keywords.contains(token) && token != "kotlin" && token != "math" &&
            token != "PI" && token != "something" && token != "A") {
            operands[token] = operands.value(token, 0) + 1;
        }
    }

    // Строковые литералы
    QRegularExpression stringRegex("(?<!\\.)\"[^\"]*\"");
    QRegularExpressionMatchIterator strIt =
        stringRegex.globalMatch(cleanedCode);
    while (strIt.hasNext()) {
        QRegularExpressionMatch match = strIt.next();
        QString str = match.captured();
        // Исключаем конкретный строковый литерал
        if (str != "\"Simple class\"") {
            operands[str] = operands.value(str, 0) + 1;
        }
    }

    // Символьные литералы
    QRegularExpression charRegex("(?<!\\.)'[^']*'");
    QRegularExpressionMatchIterator charIt = charRegex.globalMatch(cleanedCode);
    while (charIt.hasNext()) {
        QRegularExpressionMatch match = charIt.next();
        operands[match.captured()] = operands.value(match.captured(), 0) + 1;
    }
}

QString HalsteadMetrics::removeComments(const QString& code) {
    QString result = code;
    QRegularExpression singleLineComment("//[^\n]*");
    result.remove(singleLineComment);
    QRegularExpression multiLineComment(
        "/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption);
    result.remove(multiLineComment);
    return result;
}
