# Umbrella for the library projects. Only dispatches, it defines nothing itself.
#
#   ApiHeaders/ public interface headers, leaf target for both libraries below
#   Api/        public interface: factories plus the product library
#   Impl/       implementation detail: assembler, lexer, parser, compiler, VM
add_subdirectory (ApiHeaders)
add_subdirectory (Api)
add_subdirectory (Impl)
