#include <stack>
#include <queue>
#include <sstream>

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

    auto r_integer = std::make_unique<bnf::rule>("integer", std::make_unique<bnf::more>(std::make_unique<bnf::char_range>('0', '9')));
    auto r_lparen = std::make_unique<bnf::rule>("lparen", std::make_unique<bnf::literal>("("));
    auto r_rparen = std::make_unique<bnf::rule>("rparen", std::make_unique<bnf::literal>(")"));
    auto r_mul = std::make_unique<bnf::rule>("mul", std::make_unique<bnf::literal>("*"));
    auto r_div = std::make_unique<bnf::rule>("div", std::make_unique<bnf::literal>("/"));
    auto r_add = std::make_unique<bnf::rule>("add", std::make_unique<bnf::literal>("+"));
    auto r_sub = std::make_unique<bnf::rule>("sub", std::make_unique<bnf::literal>("-"));

    auto r_expr = std::make_unique<bnf::rule>("rule"); // Forward declaration

    auto r_factor= std::make_unique<bnf::rule>("factor", bnf::make<bnf::choice>(std::make_unique<bnf::rule_ref>(r_integer.get()),
                                                                                bnf::make<bnf::sequence>(std::make_unique<bnf::rule_ref>(r_lparen.get()), 
                                                                                                         std::make_unique<bnf::rule_ref>(r_expr.get()),
                                                                                                         std::make_unique<bnf::rule_ref>(r_rparen.get()))));

    auto r_term = std::make_unique<bnf::rule>("term", bnf::make<bnf::sequence>(std::make_unique<bnf::rule_ref>(r_factor.get()),
                                                                               std::make_unique<bnf::any>(bnf::make<bnf::sequence>(bnf::make<bnf::choice>(std::make_unique<bnf::rule_ref>(r_mul.get()),
                                                                                                                                                          std::make_unique<bnf::rule_ref>(r_div.get())),
                                                                                                                                   std::make_unique<bnf::rule_ref>(r_factor.get())))));

    r_expr->child = bnf::make<bnf::sequence>(std::make_unique<bnf::rule_ref>(r_term.get()),
                                             std::make_unique<bnf::any>(bnf::make<bnf::sequence>(bnf::make<bnf::choice>(std::make_unique<bnf::rule_ref>(r_add.get()),
                                                                                                                        std::make_unique<bnf::rule_ref>(r_sub.get())),
                                                                        std::make_unique<bnf::rule_ref>(r_term.get()))));

    std::cout << r_factor->to_string() << std::endl;
    std::cout << r_term->to_string() << std::endl;
    std::cout << r_expr->to_string() << std::endl;

    std::stringstream ss;
    ss << "1+2+3*4";

    // cout << r_expr.to_string() << endl;

    auto t = r_expr->match(ss);

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
