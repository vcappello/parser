#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <memory>
#include <optional>
#include <limits>
#include <functional>

namespace bnf
{
    struct rule_base; // Forward declaration
    struct rule_ref;  // Forward declaration

    struct token
    {
        std::streampos start_pos;
        std::streampos end_pos;
        rule_base *rule;
        std::vector<std::unique_ptr<token>> children;

        static std::unique_ptr<token> null_token()
        {
            return std::unique_ptr<token>();
        }

        struct iterator
        {
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = token;
            using pointer = token *;
            using reference = token &;

            struct stack_data
            {
                pointer node_ptr;
                size_t index;
            };

            iterator(pointer ptr) : m_ptr(ptr), child_index(0) {}

            reference operator*() const { return *m_ptr; }
            pointer operator->() { return m_ptr; }
            iterator &operator++()
            {
                if (m_ptr == nullptr)
                {
                    return *this;
                }

                if (!m_ptr->children.empty())
                {
                    // Store current status in the stack
                    node_stack.push({m_ptr, child_index});
                    // Go to first child
                    child_index = 0;
                    m_ptr = m_ptr->children[child_index].get();
                }
                else
                {
                    // Move up in the stack and go to next child
                    do
                    {
                        // Restore state from stack
                        auto data = node_stack.top();
                        node_stack.pop();
                        child_index = data.index;
                        m_ptr = data.node_ptr;
                    } while (!node_stack.empty() && child_index == (m_ptr->children.size() - 1));

                    if (child_index < (m_ptr->children.size() - 1))
                    {
                        // Move next and store status
                        child_index++;
                        node_stack.push({m_ptr, child_index});
                        // Next node
                        m_ptr = m_ptr->children[child_index].get();
                        child_index = 0;
                    }
                    else
                    {
                        m_ptr = nullptr;
                    }
                }
                return *this;
            }

            iterator operator++(int)
            {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const iterator &a, const iterator &b) { return a.m_ptr == b.m_ptr; };

            friend bool operator!=(const iterator &a, const iterator &b) { return a.m_ptr != b.m_ptr; };

        private:
            pointer m_ptr;
            size_t child_index;
            std::stack<stack_data> node_stack;
        };

        iterator begin() { return iterator(this); }
        iterator end() { return iterator(nullptr); }
    };

    struct rule_base
    {
        rule_base() = default;
        virtual ~rule_base() = default;

        virtual std::unique_ptr<token> match(std::istream &is) = 0;
        virtual std::string to_string() = 0;

        virtual std::unique_ptr<token> match_begin(std::istream &is)
        {
            auto t = std::make_unique<token>();
            t->start_pos = is.tellg();
            t->rule = this;

            return t;
        }

        virtual void match_fail(std::istream &is, token *t)
        {
            is.seekg(t->start_pos);
        }

        virtual void match_passed(std::istream &is, token *t)
        {
            t->end_pos = is.tellg();
        }

        std::unique_ptr<rule_ref> to_ref(); // declaration
    };

    struct terminal_rule : public rule_base
    {
        terminal_rule() = default;
        virtual ~terminal_rule() = default;
    };

    struct named_rule : public rule_base
    {
        std::string name;

        named_rule(const std::string &in_name) : name(in_name) {}
        virtual ~named_rule() = default;
    };

    struct rule_move_default
    {
        static std::unique_ptr<rule_base> move(std::unique_ptr<rule_base> in_child)
        {
            return std::move(in_child);
        }
    };

    template<typename TMovePolicy = rule_move_default>
    struct rule : public named_rule
    {
        std::unique_ptr<rule_base> child;

        rule(const std::string &in_name, std::unique_ptr<rule_base> in_child) : named_rule(in_name),
                                                                                child(std::move(TMovePolicy::move(std::move(in_child)))) {}

        rule(const std::string &in_name) : named_rule(in_name),
                                           child(nullptr) {}

        virtual ~rule() = default;

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
        rule_base *child;

        rule_ref() : child(nullptr) {}
        rule_ref(rule_base *rhs) : child(rhs) {}

        virtual ~rule_ref() = default;

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
            if (auto r = dynamic_cast<named_rule *>(child))
            {
                return r->name;
            }
            return child->to_string();
        }

        rule_ref &operator=(rule_base *rhs)
        {
            child = rhs;
            return *this;
        }
    };

    std::unique_ptr<rule_ref> rule_base::to_ref() // Implementation
    {
        return std::make_unique<rule_ref>(this);
    }

    struct literal : public terminal_rule
    {
        std::string text;

        literal(const std::string &in_text) : text(in_text) {}
        virtual ~literal() = default;

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
        virtual ~char_range() = default;

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
        virtual ~char_set() = default;

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
        std::vector<std::unique_ptr<rule_base>> children;
        virtual ~choice() = default;

        choice(std::vector<std::unique_ptr<rule_base>> in_children) : children(std::move(in_children))
        {
        }

        std::unique_ptr<token> match(std::istream &is) override
        {

            auto t = match_begin(is);

            for (auto &c : children)
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
            for (auto &c : children)
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
        std::vector<std::unique_ptr<rule_base>> children;

        sequence(std::vector<std::unique_ptr<rule_base>> in_children) : children(std::move(in_children))
        {
        }
        virtual ~sequence() = default;

        std::unique_ptr<token> match(std::istream &is) override
        {

            auto t = match_begin(is);

            for (auto &c : children)
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
            for (auto &c : children)
            {
                if (ret.length() > 1)
                    ret = ret + " ";
                ret = ret + c->to_string();
            }
            ret = ret + ")";
            return ret;
        }
    };

    struct range_any
    {
        static constexpr size_t from = 0;
        static constexpr size_t to = std::numeric_limits<size_t>::max();
    };

    struct range_opt
    {
        static constexpr size_t from = 0;
        static constexpr size_t to = 1;
    };

    struct range_more
    {
        static constexpr size_t from = 1;
        static constexpr size_t to = std::numeric_limits<size_t>::max();
    };

    template <typename T>
    struct repeat : public rule_base
    {
        std::unique_ptr<rule_base> child;
        T range;

        repeat(std::unique_ptr<rule_base> in_rule_base) : child(std::move(in_rule_base)) {}
        virtual ~repeat() = default;

        std::unique_ptr<token> match(std::istream &is) override
        {
            auto t = match_begin(is);
            size_t count = 0;
            while (true)
            {
                auto tc = child->match(is);
                if (!tc)
                {
                    if (count >= T::from && count <= T::to)
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

            if (T::from == 0 && T::to == 1)
                ret = ret + "?";
            if (T::from == 0 && T::to > 1)
                ret = ret + "*";
            if (T::from == 1 && T::to > 1)
                ret = ret + "+";

            return ret;
        }
    };

    using any = repeat<range_any>;
    using opt = repeat<range_opt>;
    using more = repeat<range_more>;

    template <typename T>
    struct is_rule_container : std::false_type
    {
    };

    template <>
    struct is_rule_container<choice> : std::true_type
    {
    };
    template <>
    struct is_rule_container<sequence> : std::true_type
    {
    };

    template <typename T, typename... Args>
    std::enable_if_t<!is_rule_container<T>::value, std::unique_ptr<T>> make(Args &&...args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    std::enable_if_t<is_rule_container<T>::value, std::unique_ptr<T>> make(Args &&...args)
    {
        std::vector<std::unique_ptr<rule_base>> vec;
        vec.reserve(sizeof...(Args));
        (vec.emplace_back(std::forward<Args>(args)), ...);
        return std::make_unique<T>(std::move(vec));
    }

    static std::unique_ptr<any> whitespace = make<any>(make<char_set>(" \t"));

    struct skip_whitespace
    {
        static std::unique_ptr<rule_base> move(std::unique_ptr<rule_base> in_child)
        {        
            auto wsr = make<sequence>(whitespace->to_ref(),
                                      std::move(in_child),
                                      whitespace->to_ref());

            return std::move(wsr);
        }
    };

    using rulea = rule<>;
    using rulew = rule<skip_whitespace>;
}
