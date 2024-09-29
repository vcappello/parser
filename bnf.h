#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <vector>
#include <limits>
#include <functional>

namespace bnf
{
    struct rule_base;

    struct token
    {
        std::streampos start;
        std::streampos end;
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
        virtual std::unique_ptr<token> match(std::istream &is) = 0;
        virtual std::string to_string() = 0;

        virtual std::unique_ptr<token> match_begin(std::istream &is)
        {
            auto t = std::make_unique<token>();
            t->start = is.tellg();
            t->rule = this;

            return t;
        }

        virtual void match_fail(std::istream &is, token *t)
        {
            is.seekg(t->start);
        }

        virtual void match_passed(std::istream &is, token *t)
        {
            t->end = is.tellg();
        }
    };

    struct terminal_rule : public rule_base
    {
    };

    struct rule : public rule_base
    {
        std::string name;
        rule_base *child;

        rule(const std::string &in_name, rule_base *in_child) : name(in_name), child(in_child) {}
        rule(const std::string &in_name) : name(in_name), child(nullptr) {}

        std::unique_ptr<token> match(std::istream &is) override
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

        std::string to_string() override
        {
            return name + " := " + child->to_string() + "\n";
        }
    };

    struct rule_ref : public rule_base
    {
        std::string name;
        rule_base *child;

        rule_ref() : child(nullptr) {}
        rule_ref(const rule &rhs) : name(rhs.name), child(rhs.child) {}

        std::unique_ptr<token> match(std::istream &is) override
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

        std::string to_string() override
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
        std::string text;

        literal(const std::string &in_text) : text(in_text) {}

        std::unique_ptr<token> match(std::istream &is) override
        {
            if (is.eof())
                return token::null_token();
            auto t = match_begin(is);
            std::string buf(text.size(), '\0');
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

        std::string to_string() override
        {
            std::string ret;
            ret = "\"" + text + "\"";
            return ret;
        }
    };

    struct char_range : public terminal_rule
    {
        char low;
        char high;

        char_range(const char in_low, const char in_high) : low(in_low), high(in_high) {}

        std::unique_ptr<token> match(std::istream &is) override
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

        std::string to_string() override
        {
            return std::string("[") + low + "-" + high + "]";
        }
    };

    struct char_set : public terminal_rule
    {
        std::string cset;

        char_set(const std::string &in_cset) : cset(in_cset) {}

        std::unique_ptr<token> match(std::istream &is) override
        {
            if (is.eof())
                return token::null_token();
            auto t = match_begin(is);
            char c;
            is.get(c);
            if (!is.good() || cset.find(c) == std::string::npos)
            {
                is.clear();
                match_fail(is, t.get());
                return token::null_token();
            }
            match_passed(is, t.get());
            return t;
        }

        std::string to_string() override
        {
            return std::string("[") + cset + "]";
        }
    };

    struct choice : public rule_base
    {
        std::vector<rule_base *> children;

        choice(std::initializer_list<rule_base *> in_children) : children(in_children)
        {
        }

        std::unique_ptr<token> match(std::istream &is) override
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

        std::string to_string() override
        {
            std::string ret;
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
        std::vector<rule_base *> children;

        sequence(std::initializer_list<rule_base *> in_children) : children(in_children)
        {
        }

        std::unique_ptr<token> match(std::istream &is) override
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

        std::string to_string() override
        {
            std::string ret;
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

        std::unique_ptr<token> match(std::istream &is) override
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

        std::string to_string() override
        {
            std::string ret = child->to_string();

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
