

import ast
from IR import ir

def parse_code_to_ast(code):
    """
    Parses the given code into an Abstract Syntax Tree (AST).
    
    Args:
        code (str): The source code to be parsed.
        
    Returns:
        ast.AST: The root of the AST representing the parsed code.
    """
    return ast.parse(code)

tree = parse_code_to_ast(open("module.py").read())

