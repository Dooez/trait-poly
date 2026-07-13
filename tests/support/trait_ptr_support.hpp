#pragma once

struct counts {
    int alive     = 0;
    int destroyed = 0;
};

struct read_trait {
    auto value() const -> int;
};

struct write_trait : read_trait {
    auto set(int value) -> void;
};

struct node {
    counts* tracker = nullptr;
    int     stored  = 0;

    node(counts& tracker, int value)
    : tracker(&tracker)
    , stored(value) {
        ++tracker.alive;
    }

    node(node const&)            = delete;
    node& operator=(node const&) = delete;

    ~node() {
        --tracker->alive;
        ++tracker->destroyed;
    }

    auto value() const -> int {
        return stored;
    }

    auto set(int value) -> void {
        stored = value;
    }
};

struct read_node {
    counts* tracker = nullptr;
    int     stored  = 0;

    read_node(counts& tracker, int value)
    : tracker(&tracker)
    , stored(value) {
        ++tracker.alive;
    }

    read_node(read_node const&)            = delete;
    read_node& operator=(read_node const&) = delete;

    ~read_node() {
        --tracker->alive;
        ++tracker->destroyed;
    }

    auto value() const -> int {
        return stored;
    }
};
