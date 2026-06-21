#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// Декодер UTF-8 в UTF-32 для корректной работы с кириллицей и спецсимволами
std::vector<uint32_t> utf8_to_utf32(const std::string& s) {
    std::vector<uint32_t> res;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        if ((c & 0x80) == 0) {
            res.push_back(c); i++;
        } else if ((c & 0xE0) == 0xC0 && i+1 < s.size()) {
            res.push_back(((c & 0x1F) << 6) | (s[i+1] & 0x3F)); i += 2;
        } else if ((c & 0xF0) == 0xE0 && i+2 < s.size()) {
            res.push_back(((c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F)); i += 3;
        } else if ((c & 0xF8) == 0xF0 && i+3 < s.size()) {
            res.push_back(((c & 0x07) << 18) | ((s[i+1] & 0x3F) << 12) | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F)); i += 4;
        } else {
            res.push_back(0xFFFD); i++;
        }
    }
    return res;
}

enum NodeType { NT_EMPTY, NT_LITERAL, NT_CONCAT, NT_ALT, NT_STAR };

struct Node {
    NodeType type = NT_EMPTY;
    uint32_t ch = 0;
    std::vector<std::shared_ptr<Node>> children;
    Node(NodeType t, uint32_t c = 0) : type(t), ch(c) {}
};
using NodePtr = std::shared_ptr<Node>;

class RegexParser {
    const std::vector<uint32_t>& pattern;
    size_t pos = 0;
    std::string error;

    uint32_t peek() const { return pos < pattern.size() ? pattern[pos] : 0; }
    uint32_t consume() { return pos < pattern.size() ? pattern[pos++] : 0; }

    NodePtr parse_expr() {
        NodePtr left = parse_term();
        if (!left) return nullptr;
        while (peek() == '|') {
            consume();
            NodePtr right = parse_term();
            if (!right) return nullptr;
            NodePtr alt = std::make_shared<Node>(NT_ALT);
            alt->children.push_back(left);
            alt->children.push_back(right);
            left = alt;
        }
        return left;
    }

    NodePtr parse_term() {
        std::vector<NodePtr> parts;
        while (pos < pattern.size() && peek() != '|' && peek() != ')') {
            NodePtr f = parse_factor();
            if (!f) return nullptr;
            parts.push_back(f);
        }
        if (parts.empty()) return std::make_shared<Node>(NT_EMPTY);
        if (parts.size() == 1) return parts[0];
        NodePtr concat = std::make_shared<Node>(NT_CONCAT);
        concat->children = std::move(parts);
        return concat;
    }

    NodePtr parse_factor() {
        NodePtr atom = parse_atom();
        if (!atom) return nullptr;
        if (peek() == '*') {
            consume();
            NodePtr star = std::make_shared<Node>(NT_STAR);
            star->children.push_back(atom);
            return star;
        }
        return atom;
    }

    NodePtr parse_atom() {
        if (pos >= pattern.size()) { error = "Unexpected end"; return nullptr; }
        uint32_t c = peek();
        if (c == '\\') {
            consume();
            if (pos >= pattern.size()) { error = "Trailing backslash"; return nullptr; }
            return std::make_shared<Node>(NT_LITERAL, consume());
        } else if (c == '(') {
            consume();
            NodePtr expr = parse_expr();
            if (!expr) return nullptr;
            if (pos >= pattern.size() || peek() != ')') {
                error = "Missing ')'"; return nullptr;
            }
            consume();
            return expr;
        } else if (c == ')' || c == '|' || c == '*') {
            error = "Invalid character in context"; return nullptr;
        } else {
            consume();
            return std::make_shared<Node>(NT_LITERAL, c);
        }
    }

public:
    RegexParser(const std::string& re) : pattern(utf8_to_utf32(re)) {}
    NodePtr parse() {
        NodePtr root = parse_expr();
        if (!error.empty() || pos < pattern.size()) return nullptr;
        return root;
    }
    const std::string& getError() const { return error; }
};

// Матчер: возвращает индекс конца совпадения или -1 при неудаче.
// Жадный для *, левый приоритет для |, без бэктрекинга по оставшемуся тексту.
int match(const NodePtr& node, const std::vector<uint32_t>& text, size_t pos) {
    switch (node->type) {
        case NT_EMPTY: return (pos <= text.size()) ? pos : -1;
        case NT_LITERAL:
            return (pos < text.size() && text[pos] == node->ch) ? pos + 1 : -1;
        case NT_CONCAT: {
            size_t cur = pos;
            for (const auto& child : node->children) {
                int end = match(child, text, cur);
                if (end == -1) return -1;
                cur = end;
            }
            return cur;
        }
        case NT_ALT: {
            int res = match(node->children[0], text, pos);
            if (res != -1) return res;
            return match(node->children[1], text, pos);
        }
        case NT_STAR: {
            std::vector<int> positions;
            positions.push_back(pos);
            while (true) {
                int next = match(node->children[0], text, positions.back());
                if (next == -1 || next == positions.back()) break;
                positions.push_back(next);
            }
            return positions.back();
        }
    }
    return -1;
}

int count_regex_matches(const std::string& re, const std::string& text) {
    RegexParser parser(re);
    NodePtr root = parser.parse();
    if (!root || !parser.getError().empty()) return -1;

    auto text_utf = utf8_to_utf32(text);
    int count = 0;
    size_t i = 0;
    while (i <= text_utf.size()) {
        int end = match(root, text_utf, i);
        if (end == -1) { i++; continue; }
        count++;
        i = (end == i) ? i + 1 : end; // защита от бесконечного цикла на пустых совпадениях
    }
    return count;
}

int main(int argc, char* argv[]) {
    if (argc != 4) return 1;
    std::ifstream re_in(argv[1]), text_in(argv[2]);
    std::ofstream out(argv[3]);
    if (!re_in || !text_in || !out) return 1;

    std::vector<std::string> regexes;
    for (std::string line; std::getline(re_in, line); ) {
        // Убираем \r и \n, пропускаем пустые
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (!line.empty()) regexes.push_back(line);
    }

    std::string line;
    while (std::getline(text_in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;

        for (size_t i = 0; i < regexes.size(); ++i) {
            out << count_regex_matches(regexes[i], line);
            if (i + 1 < regexes.size()) out << ' ';
        }
        out << '\n';
    }
    return 0;
}
