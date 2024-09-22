#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <optional>
#include <vector>
#include <limits>
#include <stack>
#include <functional>

using namespace std;

namespace bnf
{
    struct rule_base;

    struct token
    {
        streampos start;
        streampos end;
        rule_base *rule;
        std::vector<std::unique_ptr<token>> children;

        static std::unique_ptr<token> null_token()
        {
            return std::unique_ptr<token>();
        }

        void for_each(std::function<void(token *)> fn)
        {
            fn(this);

            for (auto &c : children)
            {
                c->for_each(fn);
            }
        }
    };

    struct rule_base
    {
        virtual std::unique_ptr<token> match(istream &is) = 0;
        virtual string to_string() = 0;

        virtual std::unique_ptr<token> match_begin(istream &is)
        {
            auto t = std::make_unique<token>();
            t->start = is.tellg();
            t->rule = this;

            return t;
        }

        virtual void match_fail(istream &is, token *t)
        {
            // cout << rule->to_string() << " FAIL" << endl;

            is.seekg(t->start);
        }

        virtual void match_passed(istream &is, token *t)
        {
            // cout << rule->to_string() << " Ok" << endl;

            t->end = is.tellg();
            // st.parent->children.emplace_back(t);
        }
    };

    struct terminal_rule : public rule_base
    {
    };

    struct rule : public rule_base
    {
        string name;
        rule_base *child;

        rule(const string &in_name, rule_base *in_child) : name(in_name), child(in_child) {}
        rule(const string &in_name) : name(in_name), child(nullptr) {}

        std::unique_ptr<token> match(istream &is) override
        {
            auto t = match_begin(is);
            if (auto tc = child->match(is))
            {
                t->children.emplace_back(std::move(tc));
                match_passed(is, t.get());
                return t;
            }
            match_fail(is, t.get());
            return token::null_token();
        }

        string to_string() override
        {
            return name + " := " + child->to_string() + "\n";
        }
    };

    struct rule_ref : public rule_base
    {
        string name;
        rule_base *child;

        rule_ref() : child(nullptr) {}
        rule_ref(const rule &rhs) : name(rhs.name), child(rhs.child) {}

        std::unique_ptr<token> match(istream &is) override
        {
            auto t = match_begin(is);
            if (auto tc = child->match(is))
            {
                t->children.emplace_back(std::move(tc));
                match_passed(is, t.get());
                return t;
            }
            match_fail(is, t.get());
            return token::null_token();
        }

        string to_string() override
        {
            return name;
        }

        rule_ref &operator=(const rule &rhs)
        {
            this->name = rhs.name;
            this->child = rhs.child;
            return *this;
        }
    };

    struct literal : public terminal_rule
    {
        string text;

        literal(const string &in_text) : text(in_text) {}

        std::unique_ptr<token> match(istream &is) override
        {
            if (is.eof())
                return token::null_token();
            auto t = match_begin(is);
            string buf(text.size(), '\0');
            is.read(&buf[0], text.size());
            if (!is.good() || buf != text)
            {
                is.clear();
                match_fail(is, t.get());
                return token::null_token();
            }
            match_passed(is, t.get());
            return t;
        }

        string to_string() override
        {
            string ret;
            ret = "\"" + text + "\"";
            return ret;
        }
    };

    struct char_range : public terminal_rule
    {
        char low;
        char high;

        char_range(const char in_low, const char in_high) : low(in_low), high(in_high) {}

        std::unique_ptr<token> match(istream &is) override
        {
            if (is.eof())
                return token::null_token();
            auto t = match_begin(is);
            char c;
            is.get(c);
            if (!is.good() || c < low || c > high)
            {
                is.clear();
                match_fail(is, t.get());
                return token::null_token();
            }
            match_passed(is, t.get());
            return t;
        }

        string to_string() override
        {
            return string("[") + low + "-" + high + "]";
        }
    };

    struct char_set : public terminal_rule
    {
        string cset;

        char_set(const string &in_cset) : cset(in_cset) {}

        std::unique_ptr<token> match(istream &is) override
        {
            if (is.eof())
                return token::null_token();
            auto t = match_begin(is);
            char c;
            is.get(c);
            if (!is.good() || cset.find(c) == string::npos)
            {
                is.clear();
                match_fail(is, t.get());
                return token::null_token();
            }
            match_passed(is, t.get());
            return t;
        }

        string to_string() override
        {
            return string("[") + cset + "]";
        }
    };

    struct choice : public rule_base
    {
        vector<rule_base *> children;

        choice(initializer_list<rule_base *> in_children) : children(in_children)
        {
        }

        std::unique_ptr<token> match(istream &is) override
        {

            auto t = match_begin(is);

            for (auto c : children)
            {
                auto tc = c->match(is);
                if (tc)
                {
                    t->children.emplace_back(std::move(tc));
                    match_passed(is, t.get());
                    return t;
                }
            }

            match_fail(is, t.get());
            return token::null_token();
        }

        string to_string() override
        {
            string ret;
            ret = "(";
            for (auto c : children)
            {
                if (ret.length() > 1)
                    ret = ret + "|";
                ret = ret + c->to_string();
            }
            ret = ret + ")";
            return ret;
        }
    };

    struct sequence : public rule_base
    {
        vector<rule_base *> children;

        sequence(initializer_list<rule_base *> in_children) : children(in_children)
        {
        }

        std::unique_ptr<token> match(istream &is) override
        {

            auto t = match_begin(is);

            for (auto c : children)
            {
                auto tc = c->match(is);
                if (!tc)
                {
                    match_fail(is, t.get());
                    return token::null_token();
                }
                t->children.emplace_back(std::move(tc));
            }

            match_passed(is, t.get());
            return t;
        }

        string to_string() override
        {
            string ret;
            ret = "(";
            for (auto c : children)
            {
                if (ret.length() > 1)
                    ret = ret + " ";
                ret = ret + c->to_string();
            }
            ret = ret + ")";
            return ret;
        }
    };

    template <size_t ML, size_t MM>
    struct repeat : public rule_base
    {
        rule_base *child;
        size_t min = ML;
        size_t max = MM;

        repeat(rule_base *in_rule_base) : child(in_rule_base) {}

        std::unique_ptr<token> match(istream &is) override
        {
            auto t = match_begin(is);
            size_t count = 0;
            while (true)
            {
                auto tc = child->match(is);
                if (!tc)
                {
                    if (count >= min && count <= max)
                    {
                        match_passed(is, t.get());
                        return t;
                    }
                    match_fail(is, t.get());
                    return token::null_token();
                }
                else
                {
                    t->children.emplace_back(std::move(tc));
                }
                count++;
            }
            match_fail(is, t.get());
            return token::null_token();
        }

        string to_string() override
        {
            string ret = child->to_string();

            if (min == 0 && max == 1)
                ret = ret + "?";
            if (min == 0 && max > 1)
                ret = ret + "*";
            if (min == 1 && max > 1)
                ret = ret + "+";

            return ret;
        }
    };

    using any = repeat<0, std::numeric_limits<size_t>::max()>;
    using opt = repeat<0, 1>;
    using more = repeat<1, std::numeric_limits<size_t>::max()>;
}

void test_out(bnf::token *t, istream &is)
{
    if (auto r = dynamic_cast<bnf::rule *>(t->rule))
    {
        cout << r->name << " " << t->start << ", " << t->end;
        if (t->end != -1)
        {
            streamsize len = t->end - t->start;
            string buf(len, '\0');
            is.seekg(t->start);
            is.read(&buf[0], len);
            cout << "(" << len << ")" << " '" << buf << "'";
        }
        cout << endl;
    }

    for (auto &tc : t->children)
    {
        test_out(tc.get(), is);
    }
}

void shunting_yard_algorithm(bnf::token *t)
{
}

void test_complex()
{
    cout << "TEST_COMPLEX" << endl;

    bnf::char_range i_digit('0', '9');
    bnf::more i_integer(&i_digit);
    bnf::rule r_integer("integer", &i_integer);

    bnf::literal i_lparen("(");
    bnf::rule r_lparen("lparen", &i_lparen);

    bnf::literal i_rparen(")");
    bnf::rule r_rparen("rparen", &i_rparen);

    bnf::literal i_mul("*");
    bnf::rule r_mul("mul", &i_mul);

    bnf::literal i_div("/");
    bnf::rule r_div("div", &i_div);

    bnf::literal i_add("+");
    bnf::rule r_add("add", &i_add);

    bnf::literal i_sub("-");
    bnf::rule r_sub("sub", &i_sub);

    bnf::rule_ref rr_expr;

    bnf::sequence i_factor_1{&r_lparen, &rr_expr, &r_rparen};
    bnf::choice i_factor_2{&r_integer, &i_factor_1};
    bnf::rule r_factor("factor", &i_factor_2);

    bnf::choice i_term_1{&r_mul, &r_div};
    bnf::sequence i_term_2{&i_term_1, &r_factor};
    bnf::any i_term_3(&i_term_2);
    bnf::sequence i_term_4{&r_factor, &i_term_3};
    bnf::rule r_term("term", &i_term_4);

    bnf::choice i_expr_1{&r_add, &r_sub};
    bnf::sequence i_expr_2{&i_expr_1, &r_term};
    bnf::any i_expr_3(&i_expr_2);
    bnf::sequence i_expr_4{&r_term, &i_expr_3};

    bnf::rule r_expr("expr", &i_expr_4);
    rr_expr = r_expr;

    stringstream ss;
    ss << "(1+2)*3";

    // cout << r_expr.to_string() << endl;

    auto t = r_expr.match(ss);

    ss.clear();

    cout << "Match token: ";
    if (t)
    {
        cout << "Passed" << endl;
        t->for_each([&](bnf::token *t)
            { 
                if (auto r = dynamic_cast<bnf::rule *>(t->rule))
                {
                    cout << r->name << " " << t->start << ", " << t->end;
                    if (t->end != -1)
                    {
                        streamsize len = t->end - t->start;
                        string buf(len, '\0');
                        ss.seekg(t->start);
                        ss.read(&buf[0], len);        
                        cout << "(" << len << ")" << " '" << buf << "'"; 
                    }
                    cout << endl;
                } 
            });
    }
    else
    {
        cout << "NOT passed" << endl;
    }
}

int main()
{
    test_complex();

    return 0;
}
