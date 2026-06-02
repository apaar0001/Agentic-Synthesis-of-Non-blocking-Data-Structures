"""
C++ CodeBLEU evaluation package.

This package provides tools for evaluating C++ concurrent data structures
using multi-layer scoring including annotation matching, CodeBLEU, and
concurrency primitives checking.
"""

from .cpp_annotation import score_cpp_annotation, score_cpp_file

__all__ = ['score_cpp_annotation', 'score_cpp_file']
