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

    /* Interface to a Viewer that's specifically designed for interactively editing
     * the underlying graph structure.
     */
    class Editor {
    public:
        /* Builds an editor that hooks into the specified Viewer. */
        Editor(std::shared_ptr<Viewer> viewer);

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

        /* Deletes the given node/edge. */
        void deleteNode(Node* node);
        void deleteEdge(Edge* edge);

        /* Retrieve the underlying editor. */
        std::shared_ptr<Viewer> viewer();

    private:
        std::shared_ptr<Viewer> mViewer;
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
    };
}
