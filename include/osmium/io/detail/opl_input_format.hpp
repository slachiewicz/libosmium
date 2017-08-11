#ifndef OSMIUM_IO_DETAIL_OPL_INPUT_FORMAT_HPP
#define OSMIUM_IO_DETAIL_OPL_INPUT_FORMAT_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2017 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <osmium/io/detail/input_format.hpp>
#include <osmium/io/detail/opl_parser_functions.hpp>
#include <osmium/io/file_format.hpp>
#include <osmium/io/header.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/thread/util.hpp>

namespace osmium {

    namespace io {

        namespace detail {

            class OPLParser : public Parser {

                osmium::memory::Buffer m_buffer{1024*1024};
                const char* m_data = nullptr;
                uint64_t m_line_count = 0;

                void maybe_flush() {
                    if (m_buffer.committed() > 800*1024) {
                        osmium::memory::Buffer buffer{1024*1024};
                        using std::swap;
                        swap(m_buffer, buffer);
                        send_to_output_queue(std::move(buffer));

                    }
                }

                void parse_line() {
                    if (opl_parse_line(m_line_count, m_data, m_buffer, read_types())) {
                        maybe_flush();
                    }
                    ++m_line_count;
                }

            public:

                explicit OPLParser(parser_arguments& args) :
                    Parser(args) {
                    set_header_value(osmium::io::Header{});
                }

                ~OPLParser() noexcept final = default;

                void run() final {
                    osmium::thread::set_thread_name("_osmium_opl_in");

                    std::string rest;
                    while (!input_done()) {
                        std::string input{get_input()};
                        std::string::size_type ppos = 0;

                        if (!rest.empty()) {
                            ppos = input.find('\n');
                            if (ppos == std::string::npos) {
                                rest.append(input);
                                continue;
                            }
                            rest.append(input.substr(0, ppos));
                            m_data = rest.data();
                            parse_line();
                            rest.clear();
                        }

                        std::string::size_type pos = input.find('\n', ppos);
                        while (pos != std::string::npos) {
                            m_data = &input[ppos];
                            input[pos] = '\0';
                            parse_line();
                            ppos = pos + 1;
                            if (ppos >= input.size()) {
                                break;
                            }
                            pos = input.find('\n', ppos);
                        }
                        rest = input.substr(ppos);
                    }

                    if (!rest.empty()) {
                        m_data = rest.data();
                        parse_line();
                    }

                    if (m_buffer.committed() > 0) {
                        send_to_output_queue(std::move(m_buffer));
                    }
                }

            }; // class OPLParser

            // we want the register_parser() function to run, setting
            // the variable is only a side-effect, it will never be used
            const bool registered_opl_parser = ParserFactory::instance().register_parser(
                file_format::opl,
                [](parser_arguments& args) {
                    return std::unique_ptr<Parser>(new OPLParser{args});
            });

            // dummy function to silence the unused variable warning from above
            inline bool get_registered_opl_parser() noexcept {
                return registered_opl_parser;
            }

        } // namespace detail

    } // namespace io

} // namespace osmium


#endif // OSMIUM_IO_DETAIL_OPL_INPUT_FORMAT_HPP
