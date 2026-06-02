from __future__ import annotations
from langgraph.graph import StateGraph, START, END
from java_react import *


def build_graph():
	graph = StateGraph(GraphState)
	# Phase nodes
	graph.add_node("generate_seq", node_generate_seq)
	graph.add_node("test_code_seq", node_test_code_seq)
	graph.add_node("prepare_reprompt_seq", node_prepare_reprompt_seq)
	graph.add_node("switch_to_concurrent", node_switch_to_concurrent)
	graph.add_node("generate_con", node_generate_con)
	graph.add_node("test_conc", node_test_code_conc)
	graph.add_node("verify_structural", node_verify_structural)
	graph.add_node("prepare_reprompt_structural", node_prepare_reprompt_structural)
	graph.add_node("prepare_reprompt_con", node_prepare_reprompt_con)
	graph.add_node("log_final", node_log_success)
	graph.add_node("log_final_failure", node_log_failure)

	# --- Sequential Phase ---
	graph.set_entry_point("generate_seq")
	graph.add_edge("generate_seq", "test_code_seq")
	
	graph.add_conditional_edges("test_code_seq", route_after_test_seq, {
		"switch_to_concurrent": "switch_to_concurrent",
		"prepare_reprompt_seq": "prepare_reprompt_seq",
		"log_failure": "log_final_failure",
	})
	graph.add_edge("prepare_reprompt_seq", "generate_seq")
	
	# --- Concurrent Phase ---
	# Flow: generate -> verify_structural -> test_conc -> results
	graph.add_edge("switch_to_concurrent", "generate_con")
	graph.add_edge("generate_con", "verify_structural")
	graph.add_conditional_edges("verify_structural", route_after_structural_verify, {
		"test_conc": "test_conc",
		"prepare_reprompt_structural": "prepare_reprompt_structural",
		"log_failure": "log_final_failure",
	})
	graph.add_edge("prepare_reprompt_structural", "generate_con")
	
	graph.add_conditional_edges("test_conc", route_after_test_conc, {
		"prepare_reprompt_con": "prepare_reprompt_con",
		"log_success": "log_final",
		"log_failure": "log_final_failure",
	})
	graph.add_edge("prepare_reprompt_con", "generate_con")
	
	graph.add_edge("log_final", END)
	graph.add_edge("log_final_failure", END)

	return graph.compile()


__all__ = [
	"GraphState",
	"build_graph",
]


