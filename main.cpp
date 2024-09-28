#include <stack>
#include <queue>
#include "bnf.h"

void test_out(bnf::token *root, std::istream &is)
{
    root->for_each([&](bnf::token *t)
                { 
            if (auto r = dynamic_cast<bnf::rule *>(t->rule))
            {
                std::cout << r->name << " " << t->start << ", " << t->end;
                if (t->end != -1)
                {
                    std::streamsize len = t->end - t->start;
                    std::string buf(len, '\0');
                    is.seekg(t->start);
                    is.read(&buf[0], len);
                    std::cout << "(" << len << ")" << " '" << buf << "'";
                }
                std::cout << std::endl;
            } });
}

struct expr_eval
{
    std::stack<std::string> operator_stack;
    std::queue<std::string> output_queue;

    void parse_token(bnf::token *root, std::istream &is)
    {
        root->for_each([&](bnf::token *t)
                    { 
                if (auto r = dynamic_cast<bnf::rule *>(t->rule))
                {
                    std::streamsize len = t->end - t->start;
                    std::string buf(len, '\0');
                    is.seekg(t->start);
                    is.read(&buf[0], len);  

                    if (r->name == "integer")
                    {
                        output_queue.push(buf);
                    } 
                    else if(r->name == "lparen")
                    {
                        operator_stack.push(buf);
                    }
                    else if(r->name == "add" || r->name == "sub")
                    {
                        std::vector<std::string> exit_token({ "*", "/" });
                        while (!operator_stack.empty() && operator_stack.top() != "(" && find(exit_token.begin(), exit_token.end(), operator_stack.top()) != exit_token.end())
                        {
                            output_queue.push(operator_stack.top());
                            operator_stack.pop();
                        }
                        operator_stack.push(buf);
                    }
                    else if(r->name == "mul" || r->name == "div")
                    {
                        if (!operator_stack.empty())
                        {
                            if (operator_stack.top() == "*" || operator_stack.top() == "/")
                            {
                                output_queue.push(operator_stack.top());
                                operator_stack.pop();
                            }
                        }
                        operator_stack.push(buf);
                    }
                    else if(r->name == "rparen")
                    {
                        while (!operator_stack.empty() && operator_stack.top() != "(")
                        {
                            output_queue.push(operator_stack.top());
                            operator_stack.pop();
                        }
                        if (!operator_stack.empty() && operator_stack.top() == "(")
                        {
                            operator_stack.pop();
                        }
                        else
                        {
                            // TODO: Error parentheses mismatch 
                        }
                    }
                } });
        while (!operator_stack.empty())
        {
            output_queue.push(operator_stack.top());
            operator_stack.pop();
        }
    }
    int eval(bnf::token *root, std::istream &is)
    {
        parse_token(root, is);

        while (!output_queue.empty())
        {
            auto x = output_queue.front();
            std::cout << x << " ";
            output_queue.pop();
        }
        std::cout << std::endl;
        return 0;
    }
};

void test_complex()
{
    std::cout << "TEST_COMPLEX" << std::endl;

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

    std::stringstream ss;
    ss << "1+2+3*4";

    // cout << r_expr.to_string() << endl;

    auto t = r_expr.match(ss);

    ss.clear();

    std::cout << "Match token: ";
    if (t)
    {
        std::cout << "Passed" << std::endl;
        expr_eval ev;
        ev.eval(t.get(), ss);
        //test_out(t.get(), ss);
    }
    else
    {
        std::cout << "NOT passed" << std::endl;
    }
}

int main()
{
    test_complex();

    return 0;
}
