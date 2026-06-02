package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    // Lock-freedom test helpers (auto-injected)
    private static final java.util.concurrent.atomic.AtomicBoolean _lfVictimChosen = new java.util.concurrent.atomic.AtomicBoolean(false);
    private static final ThreadLocal<Integer> _lfOpCount = ThreadLocal.withInitial(() -> 0);
    private static final ThreadLocal<Boolean> _lfRetired =
            ThreadLocal.withInitial(() -> false);

    /**
     * Decide if the *current* thread should become the victim.
     *
     * Each call increments a per-thread operation counter. Once a thread
     * has executed more than 100 operations and no victim has been chosen,
     * it atomically claims the victim role and will then stall.
     */
    private static boolean _lfShouldStall() {
        int c = _lfOpCount.get() + 1;
        _lfOpCount.set(c);
        if (c > 100 && !_lfVictimChosen.get() && _lfVictimChosen.compareAndSet(false, true)) {
            return true;
        }
        return false;
    }


    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>();
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node currentNode = root.get();
            if (currentNode == null) {
                if (root.compareAndSet(null, new Node(key))) {
                    return true;
                }
            } else {
                if (key < currentNode.key) {
                    Node leftChild = currentNode.left.getReference();
                    if (leftChild == null) {
                        if (currentNode.left.compareAndSet(null, new Node(key))) {
                            return true;
                        }
                    } else {
                        if (leftChild.key == key) {
                            return false;
                        }
                        if (leftChild.left.getReference() == null && leftChild.right.getReference() == null) {
                            if (key < leftChild.key) {
                                if (leftChild.left.compareAndSet(null, new Node(key))) {
                                    return true;
                                }
                            } else {
                                if (leftChild.right.compareAndSet(null, new Node(key))) {
                                    return true;
                                }
                            }
                        } else {
                            if (key < leftChild.key) {
                                if (leftChild.left.getReference() != null) {
                                    if (leftChild.left.getReference().key == key) {
                                        return false;
                                    }
                                }
                                if (leftChild.left.compareAndSet(null, new Node(key))) {
                                    return true;
                                }
                            } else {
                                if (leftChild.right.getReference() != null) {
                                    if (leftChild.right.getReference().key == key) {
                                        return false;
                                    }
                                }
                                if (leftChild.right.compareAndSet(null, new Node(key))) {
                                    return true;
                                }
                            }
                        }
                    }
                } else if (key > currentNode.key) {
                    Node rightChild = currentNode.right.getReference();
                    if (rightChild == null) {
                        if (currentNode.right.compareAndSet(null, new Node(key))) {
                            return true;
                        }
                    } else {
                        if (rightChild.key == key) {
                            return false;
                        }
                        if (rightChild.left.getReference() == null && rightChild.right.getReference() == null) {
                            if (key < rightChild.key) {
                                if (rightChild.left.compareAndSet(null, new Node(key))) {
                                    return true;
                                }
                            } else {
                                if (rightChild.right.compareAndSet(null, new Node(key))) {
                                    return true;
                                }
                            }
                        } else {
                            if (key < rightChild.key) {
                                if (rightChild.left.getReference() != null) {
                                    if (rightChild.left.getReference().key == key) {
                                        return false;
                                    }
                                }
                                if (rightChild.left.compareAndSet(null, new Node(key))) {
                                    return true;
                                }
                            } else {
                                if (rightChild.right.getReference() != null) {
                                    if (rightChild.right.getReference().key == key) {
                                        return false;
                                    }
                                }
                                if (rightChild.right.compareAndSet(null, new Node(key))) {
                                    return true;
                                }
                            }
                        }
                    }
                } else {
                    return false;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node currentNode = root.get();
            if (currentNode == null) {
                return false;
            } else {
                if (key < currentNode.key) {
                    Node leftChild = currentNode.left.getReference();
                    if (leftChild == null) {
                        return false;
                    } else {
                        if (leftChild.key == key) {
                            if (leftChild.left.getReference() == null && leftChild.right.getReference() == null) {
                                if (currentNode.left.compareAndSet(leftChild, null)) {
                                    return true;
                                }
                            } else {
                                if (leftChild.left.getReference() != null && leftChild.right.getReference() != null) {
                                    Node successor = leftChild.right.getReference();
                                    while (successor.left.getReference() != null) {
                                        successor = successor.left.getReference();
                                    }
                                    if (leftChild.left.compareAndSet(null, successor.left.getReference())) {
                                        leftChild.key = successor.key;
                                        if (successor.right.getReference() != null) {
                                            successor.right.compareAndSet(successor.right.getReference(), null);
                                        }
                                        return true;
                                    }
                                } else {
                                    if (leftChild.left.getReference() != null) {
                                        if (currentNode.left.compareAndSet(leftChild, leftChild.left.getReference())) {
                                            return true;
                                        }
                                    } else {
                                        if (currentNode.left.compareAndSet(leftChild, leftChild.right.getReference())) {
                                            return true;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (leftChild.left.getReference() == null && leftChild.right.getReference() == null) {
                                return false;
                            } else {
                                if (key < leftChild.key) {
                                    if (leftChild.left.getReference() != null) {
                                        if (leftChild.left.getReference().key == key) {
                                            if (leftChild.left.getReference().left.getReference() == null && leftChild.left.getReference().right.getReference() == null) {
                                                if (leftChild.left.compareAndSet(leftChild.left.getReference(), null)) {
                                                    return true;
                                                }
                                            } else {
                                                if (leftChild.left.getReference().left.getReference() != null && leftChild.left.getReference().right.getReference() != null) {
                                                    Node successor = leftChild.left.getReference().right.getReference();
                                                    while (successor.left.getReference() != null) {
                                                        successor = successor.left.getReference();
                                                    }
                                                    if (leftChild.left.getReference().left.compareAndSet(null, successor.left.getReference())) {
                                                        leftChild.left.getReference().key = successor.key;
                                                        if (successor.right.getReference() != null) {
                                                            successor.right.compareAndSet(successor.right.getReference(), null);
                                                        }
                                                        return true;
                                                    }
                                                } else {
                                                    if (leftChild.left.getReference().left.getReference() != null) {
                                                        if (leftChild.left.compareAndSet(leftChild.left.getReference(), leftChild.left.getReference().left.getReference())) {
                                                            return true;
                                                        }
                                                    } else {
                                                        if (leftChild.left.compareAndSet(leftChild.left.getReference(), leftChild.left.getReference().right.getReference())) {
                                                            return true;
                                                        }
                                                    }
                                                }
                                            }
                                        } else {
                                            return false;
                                        }
                                    } else {
                                        return false;
                                    }
                                } else {
                                    if (leftChild.right.getReference() != null) {
                                        if (leftChild.right.getReference().key == key) {
                                            if (leftChild.right.getReference().left.getReference() == null && leftChild.right.getReference().right.getReference() == null) {
                                                if (leftChild.right.compareAndSet(leftChild.right.getReference(), null)) {
                                                    return true;
                                                }
                                            } else {
                                                if (leftChild.right.getReference().left.getReference() != null && leftChild.right.getReference().right.getReference() != null) {
                                                    Node successor = leftChild.right.getReference().right.getReference();
                                                    while (successor.left.getReference() != null) {
                                                        successor = successor.left.getReference();
                                                    }
                                                    if (leftChild.right.getReference().left.compareAndSet(null, successor.left.getReference())) {
                                                        leftChild.right.getReference().key = successor.key;
                                                        if (successor.right.getReference() != null) {
                                                            successor.right.compareAndSet(successor.right.getReference(), null);
                                                        }
                                                        return true;
                                                    }
                                                } else {
                                                    if (leftChild.right.getReference().left.getReference() != null) {
                                                        if (leftChild.right.compareAndSet(leftChild.right.getReference(), leftChild.right.getReference().left.getReference())) {
                                                            return true;
                                                        }
                                                    } else {
                                                        if (leftChild.right.compareAndSet(leftChild.right.getReference(), leftChild.right.getReference().right.getReference())) {
                                                            return true;
                                                        }
                                                    }
                                                }
                                            }
                                        } else {
                                            return false;
                                        }
                                    } else {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                } else if (key > currentNode.key) {
                    Node rightChild = currentNode.right.getReference();
                    if (rightChild == null) {
                        return false;
                    } else {
                        if (rightChild.key == key) {
                            if (rightChild.left.getReference() == null && rightChild.right.getReference() == null) {
                                if (currentNode.right.compareAndSet(rightChild, null)) {
                                    return true;
                                }
                            } else {
                                if (rightChild.left.getReference() != null && rightChild.right.getReference() != null) {
                                    Node successor = rightChild.right.getReference();
                                    while (successor.left.getReference() != null) {
                                        successor = successor.left.getReference();
                                    }
                                    if (rightChild.left.compareAndSet(null, successor.left.getReference())) {
                                        rightChild.key = successor.key;
                                        if (successor.right.getReference() != null) {
                                            successor.right.compareAndSet(successor.right.getReference(), null);
                                        }
                                        return true;
                                    }
                                } else {
                                    if (rightChild.left.getReference() != null) {
                                        if (currentNode.right.compareAndSet(rightChild, rightChild.left.getReference())) {
                                            return true;
                                        }
                                    } else {
                                        if (currentNode.right.compareAndSet(rightChild, rightChild.right.getReference())) {
                                            return true;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (rightChild.left.getReference() == null && rightChild.right.getReference() == null) {
                                return false;
                            } else {
                                if (key < rightChild.key) {
                                    if (rightChild.left.getReference() != null) {
                                        if (rightChild.left.getReference().key == key) {
                                            if (rightChild.left.getReference().left.getReference() == null && rightChild.left.getReference().right.getReference() == null) {
                                                if (rightChild.left.compareAndSet(rightChild.left.getReference(), null)) {
                                                    return true;
                                                }
                                            } else {
                                                if (rightChild.left.getReference().left.getReference() != null && rightChild.left.getReference().right.getReference() != null) {
                                                    Node successor = rightChild.left.getReference().right.getReference();
                                                    while (successor.left.getReference() != null) {
                                                        successor = successor.left.getReference();
                                                    }
                                                    if (rightChild.left.getReference().left.compareAndSet(null, successor.left.getReference())) {
                                                        rightChild.left.getReference().key = successor.key;
                                                        if (successor.right.getReference() != null) {
                                                            successor.right.compareAndSet(successor.right.getReference(), null);
                                                        }
                                                        return true;
                                                    }
                                                } else {
                                                    if (rightChild.left.getReference().left.getReference() != null) {
                                                        if (rightChild.left.compareAndSet(rightChild.left.getReference(), rightChild.left.getReference().left.getReference())) {
                                                            return true;
                                                        }
                                                    } else {
                                                        if (rightChild.left.compareAndSet(rightChild.left.getReference(), rightChild.left.getReference().right.getReference())) {
                                                            return true;
                                                        }
                                                    }
                                                }
                                            }
                                        } else {
                                            return false;
                                        }
                                    } else {
                                        return false;
                                    }
                                } else {
                                    if (rightChild.right.getReference() != null) {
                                        if (rightChild.right.getReference().key == key) {
                                            if (rightChild.right.getReference().left.getReference() == null && rightChild.right.getReference().right.getReference() == null) {
                                                if (rightChild.right.compareAndSet(rightChild.right.getReference(), null)) {
                                                    return true;
                                                }
                                            } else {
                                                if (rightChild.right.getReference().left.getReference() != null && rightChild.right.getReference().right.getReference() != null) {
                                                    Node successor = rightChild.right.getReference().right.getReference();
                                                    while (successor.left.getReference() != null) {
                                                        successor = successor.left.getReference();
                                                    }
                                                    if (rightChild.right.getReference().left.compareAndSet(null, successor.left.getReference())) {
                                                        rightChild.right.getReference().key = successor.key;
                                                        if (successor.right.getReference() != null) {
                                                            successor.right.compareAndSet(successor.right.getReference(), null);
                                                        }
                                                        return true;
                                                    }
                                                } else {
                                                    if (rightChild.right.getReference().left.getReference() != null) {
                                                        if (rightChild.right.compareAndSet(rightChild.right.getReference(), rightChild.right.getReference().left.getReference())) {
                                                            return true;
                                                        }
                                                    } else {
                                                        if (rightChild.right.compareAndSet(rightChild.right.getReference(), rightChild.right.getReference().right.getReference())) {
                                                            return true;
                                                        }
                                                    }
                                                }
                                            }
                                        } else {
                                            return false;
                                        }
                                    } else {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (currentNode.left.getReference() == null && currentNode.right.getReference() == null) {
                        if (root.compareAndSet(currentNode, null)) {
                            return true;
                        }
                    } else {
                        if (currentNode.left.getReference() != null && currentNode.right.getReference() != null) {
                            Node successor = currentNode.right.getReference();
                            while (successor.left.getReference() != null) {
                                successor = successor.left.getReference();
                            }
                            if (currentNode.left.compareAndSet(null, successor.left.getReference())) {
                                currentNode.key = successor.key;
                                if (successor.right.getReference() != null) {
                                    successor.right.compareAndSet(successor.right.getReference(), null);
                                }
                                return true;
                            }
                        } else {
                            if (currentNode.left.getReference() != null) {
                                if (root.compareAndSet(currentNode, currentNode.left.getReference())) {
                                    return true;
                                }
                            } else {
                                if (root.compareAndSet(currentNode, currentNode.right.getReference())) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node currentNode = root.get();
            if (currentNode == null) {
                return false;
            } else {
                if (key < currentNode.key) {
                    Node leftChild = currentNode.left.getReference();
                    if (leftChild == null) {
                        return false;
                    } else {
                        if (leftChild.key == key) {
                            return true;
                        } else {
                            if (key < leftChild.key) {
                                if (leftChild.left.getReference() != null) {
                                    if (leftChild.left.getReference().key == key) {
                                        return true;
                                    } else {
                                        if (key < leftChild.left.getReference().key) {
                                            if (leftChild.left.getReference().left.getReference() != null) {
                                                if (leftChild.left.getReference().left.getReference().key == key) {
                                                    return true;
                                                } else {
                                                    return false;
                                                }
                                            } else {
                                                return false;
                                            }
                                        } else {
                                            if (leftChild.left.getReference().right.getReference() != null) {
                                                if (leftChild.left.getReference().right.getReference().key == key) {
                                                    return true;
                                                } else {
                                                    return false;
                                                }
                                            } else {
                                                return false;
                                            }
                                        }
                                    }
                                } else {
                                    if (leftChild.right.getReference() != null) {
                                        if (leftChild.right.getReference().key == key) {
                                            return true;
                                        } else {
                                            if (key < leftChild.right.getReference().key) {
                                                if (leftChild.right.getReference().left.getReference() != null) {
                                                    if (leftChild.right.getReference().left.getReference().key == key) {
                                                        return true;
                                                    } else {
                                                        return false;
                                                    }
                                                } else {
                                                    return false;
                                                }
                                            } else {
                                                if (leftChild.right.getReference().right.getReference() != null) {
                                                    if (leftChild.right.getReference().right.getReference().key == key) {
                                                        return true;
                                                    } else {
                                                        return false;
                                                    }
                                                } else {
                                                    return false;
                                                }
                                            }
                                        }
                                    } else {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                } else if (key > currentNode.key) {
                    Node rightChild = currentNode.right.getReference();
                    if (rightChild == null) {
                        return false;
                    } else {
                        if (rightChild.key == key) {
                            return true;
                        } else {
                            if (key < rightChild.key) {
                                if (rightChild.left.getReference() != null) {
                                    if (rightChild.left.getReference().key == key) {
                                        return true;
                                    } else {
                                        if (key < rightChild.left.getReference().key) {
                                            if (rightChild.left.getReference().left.getReference() != null) {
                                                if (rightChild.left.getReference().left.getReference().key == key) {
                                                    return true;
                                                } else {
                                                    return false;
                                                }
                                            } else {
                                                return false;
                                            }
                                        } else {
                                            if (rightChild.left.getReference().right.getReference() != null) {
                                                if (rightChild.left.getReference().right.getReference().key == key) {
                                                    return true;
                                                } else {
                                                    return false;
                                                }
                                            } else {
                                                return false;
                                            }
                                        }
                                    }
                                } else {
                                    if (rightChild.right.getReference() != null) {
                                        if (rightChild.right.getReference().key == key) {
                                            return true;
                                        } else {
                                            if (key < rightChild.right.getReference().key) {
                                                if (rightChild.right.getReference().left.getReference() != null) {
                                                    if (rightChild.right.getReference().left.getReference().key == key) {
                                                        return true;
                                                    } else {
                                                        return false;
                                                    }
                                                } else {
                                                    return false;
                                                }
                                            } else {
                                                if (rightChild.right.getReference().right.getReference() != null) {
                                                    if (rightChild.right.getReference().right.getReference().key == key) {
                                                        return true;
                                                    } else {
                                                        return false;
                                                    }
                                                } else {
                                                    return false;
                                                }
                                            }
                                        }
                                    } else {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    return true;
                }
            }
        }
    }
}