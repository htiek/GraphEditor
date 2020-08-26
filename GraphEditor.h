#pragma once

#include "GraphViewer.h"

namespace GraphEditor {
    /* Listener interface. */
    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void needsRepaint();
        virtual void isDirty();
        virtual void entitySelected(Entity* entity);
        virtual void entityHovered(Entity* entity);
    };

    /* Base type for all editors. You should not use this type directly; it's for
     * internal purposes only.
     */
    class EditorBase {
    public:
        /* Handles these mouse events to support dragging, adding new lines,
         * etc. Forward these messages to have the editor handle dragging,
         * creation of new items, etc.
         */
        void mouseDoubleClicked(double x, double y);
        void mouseMoved(double x, double y);
        void mousePressed(double x, double y);
        void mouseDragged(double x, double y);
        void mouseReleased(double x, double y);

        /* Callback registration. */
        void addListener(std::shared_ptr<Listener> listener);

        /* Draws the underlying viewer. Use this rather than manually
         * asking the viewer to draw so that we can properly highlight
         * everything.
         */
        void draw(GCanvas* canvas);

    protected:
        /* Firewalled off from the base. We promise the types will work. */
        virtual Node* newNode(const GPoint& pt) = 0;
        virtual Edge* newEdge(Node* from, Node* to) = 0;
        virtual Edge* edgeBetween(Node* from, Node* to) = 0;

    private:
        EditorBase(std::shared_ptr<ViewerBase>);
        std::shared_ptr<ViewerBase> mViewer;

        std::vector<std::shared_ptr<Listener>> mListeners;

        /* Active/hovered items. */
        Node* activeNode = nullptr;
        Node* hoverNode  = nullptr;
        Edge* activeEdge = nullptr;
        Edge* hoverEdge  = nullptr;

        /* For dragging things. */
        GPoint lastState;

        /* For dragging edges. */
        GPoint dragEdge0, dragEdge1;
        Node* edgeStart = nullptr;

        /* What kind of drag are we doing? */
        enum class DragType {
            NONE,
            NODE,
            EDGE
        };
        DragType dragType = DragType::NONE;

        void drawGraph(GCanvas* canvas);
        void drawDraggedEdge(GCanvas* canvas);

        /* Handle a drag in one of two ways. */
        void dragState(GPoint pt);
        void dragTransition(GPoint pt);

        /* Handle the mouse release when adding a new transition. */
        void finishCreatingEdge(GPoint pt);

        /* Changes the active/hover node/edge. */
        void setActive(GraphEditor::Entity* active);
        void setHover(GraphEditor::Entity* hover);

        void setActiveNode(GraphEditor::Node* state);
        void setActiveEdge(GraphEditor::Edge* transition);
        void setHoverNode(GraphEditor::Node* state);
        void setHoverEdge(GraphEditor::Edge* transition);

        void dirty();
        void requestRepaint();

        template <typename Viewer> friend class Editor;
    };

    /* Interface to a Viewer that's specifically designed for interactively editing
     * the underlying graph structure. It's parameterized on the type of the
     * underlying viewer
     */
    template <typename Viewer = GraphEditor::Viewer<>> class Editor: public EditorBase {
    public:
        using NodeType = typename Viewer::NodeType;
        using EdgeType = typename Viewer::EdgeType;

        /* Builds an editor that hooks into the specified Viewer. */
        Editor(std::shared_ptr<Viewer> viewer);

        /* Deletes the given node/edge. */
        void deleteNode(NodeType* node);
        void deleteEdge(EdgeType* edge);

        /* Retrieve the underlying editor. */
        std::shared_ptr<Viewer> viewer();

    protected:
        virtual Node* newNode(const GPoint& pt) override;
        virtual Edge* newEdge(Node* from, Node* to) override;
        virtual Edge* edgeBetween(Node* from, Node* to) override;

    private:
        std::shared_ptr<Viewer> mViewer;
    };

    template <typename Viewer>
    Editor<Viewer>::Editor(std::shared_ptr<Viewer> viewer) : EditorBase(viewer), mViewer(viewer) {
        // Handled above
    }

    template <typename Viewer>
    std::shared_ptr<Viewer> Editor<Viewer>::viewer() {
        return mViewer;
    }

    template <typename Viewer>
    void Editor<Viewer>::deleteNode(NodeType* node) {
        /* Remove the state. */
        mViewer->removeNode(node);

        /* If this was the active node, deselect it. */
        if (node == activeNode) setActive(nullptr);
        if (node == hoverNode)  setHover(nullptr);

        /* Deselect the active transition; it may no longer be valid! */
        if (activeEdge) setActive(nullptr);
        if (hoverEdge)  setHover(nullptr);

        dirty();
    }

    template <typename Viewer>
    void Editor<Viewer>::deleteEdge(EdgeType* edge) {
        /* Remove from the list of transitions. */
        mViewer->removeEdge(edge);

        if (activeEdge == edge) setActive(nullptr);
        if (hoverEdge  == edge) setHover(nullptr);
        dirty();
    }

    template <typename Viewer>
    Node* Editor<Viewer>::newNode(const GPoint& pt) {
        return mViewer->newNode(pt);
    }

    template <typename Viewer>
    Edge* Editor<Viewer>::newEdge(Node* src, Node* dst) {
        return mViewer->newEdge(static_cast<NodeType*>(src), static_cast<NodeType*>(dst));
    }

    template <typename Viewer>
    Edge* Editor<Viewer>::edgeBetween(Node* src, Node* dst) {
        return mViewer->edgeBetween(static_cast<NodeType*>(src), static_cast<NodeType*>(dst));
    }
}
