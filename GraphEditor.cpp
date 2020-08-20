#include "GraphEditor.h"
#include "GVector.h"
using namespace std;

namespace GraphEditor {
    namespace {
        /* Active state is displayed with a highlight color. */
        const string kActiveStateColor = "#ffd320"; // Slide highlight color

        /* Hovered state is displayed with a thicker, special border. */
        const string kHoverBorderColor = "blue";
        const double kHoverBorderWidth = 16.0 / 1000; // 8px on a 1000px window

        /* How far, in radians, you need to travel before it counts as a self-loop. */
        const double kSelfTransitionThreshold = M_PI / 3;

        const double kNewTransitionWidth = 3;
        const string kNewTransitionColor = "red";

        const string kActiveTransitionColor = "#ff950e";
        const double kActiveTransitionWidth = GraphEditor::kEdgeTolerance;
        const string kHoverTransitionColor = "blue"; // Slide highlight dark color
    }

    Editor::Editor(shared_ptr<Viewer> viewer) : mViewer(viewer) {
        // Handled above
    }

    std::shared_ptr<Viewer> Editor::viewer() {
        return mViewer;
    }

    void Editor::setActive(Entity* active) {
        if (auto* node = dynamic_cast<Node*>(active)) {
            setActiveNode(node);
        } else if (auto* edge = dynamic_cast<Edge*>(active)) {
            setActiveEdge(edge);
        } else {
            setActiveNode(nullptr);
            setActiveEdge(nullptr);
        }

        /* Let folks know about this one. */
        for (auto listener: mListeners) {
            listener->entitySelected(active);
        }
    }

    void Editor::setHover(Entity* hover) {
        if (auto* node = dynamic_cast<GraphEditor::Node*>(hover)) {
            setHoverNode(node);
        } else if (auto* edge = dynamic_cast<GraphEditor::Edge*>(hover)) {
            setHoverEdge(edge);
        } else {
            setHoverNode(nullptr);
            setHoverEdge(nullptr);
        }

        /* Let folks know about this one. */
        for (auto listener: mListeners) {
            listener->entityHovered(hover);
        }
    }

    void Editor::setActiveNode(Node* state) {
        if (activeNode != state) requestRepaint();
        activeNode = state;

        if (activeNode) {
            activeEdge = nullptr;
        }
    }

    void Editor::setActiveEdge(Edge* transition) {
        if (activeEdge != transition) requestRepaint();
        activeEdge = transition;

        if (activeEdge != nullptr) {
            activeNode = nullptr;
        }
    }

    void Editor::setHoverNode(Node* state) {
        if (hoverNode != state) requestRepaint();
        hoverNode = state;

        if (hoverNode) {
            hoverEdge = nullptr;
        }
    }

    void Editor::setHoverEdge(Edge* transition) {
        if (hoverEdge != transition) requestRepaint();
        hoverEdge = transition;

        if (hoverEdge) {
            hoverNode = nullptr;
        }
    }

    void Editor::mouseDoubleClicked(double x, double y) {
        GPoint pos = mViewer->graphicsToWorld(GPoint{x, y});

        /* Anything there? If so, don't do anything. */
        if (mViewer->nodeAt(pos)) return;
        if (mViewer->edgeAt(pos)) return;

        auto state = mViewer->newNode(pos);

        setHover(state);
        setActive(state);
        requestRepaint();
        dirty();
    }

    void Editor::mouseMoved(double x, double y) {
        /* Skip this if we're dragging the mouse. */
        if (dragType != DragType::NONE) return;

        GPoint pt = mViewer->graphicsToWorld(GPoint{x, y});

        /* See if we hit a state. */
        if (auto over = mViewer->nodeAt(pt)) {
            setHover(over);
        } else if (auto over = mViewer->edgeAt(pt)) {
            setHover(over);
        } else {
            setHover(nullptr);
        }
    }

    namespace {
        bool isCloseTo(GPoint p0, GPoint p1, double distance) {
            double dx = p0.getX() - p1.getX();
            double dy = p0.getY() - p1.getY();

            return dx * dx + dy * dy <= distance * distance;
        }
    }

    void Editor::mousePressed(double x, double y) {
        GPoint pt = mViewer->graphicsToWorld(GPoint{x, y});

        /* Did we hit a state? */
        if (auto over = mViewer->nodeAt(pt)) {
            setActive(over);

            /* Compute the distance to the center of the state. */
            if (isCloseTo(pt, hoverNode->position(), GraphEditor::kNodeRadius - GraphEditor::kEdgeTolerance)) {
                /* Initiate a state drag. */
                lastState = pt;
                dragType = DragType::NODE;
            } else {
                dragEdge0 = dragEdge1 = pt;
                edgeStart = hoverNode;
                dragType = DragType::EDGE;
            }
        }
        /* Did we hit a transition? */
        else if (auto over = mViewer->edgeAt(pt)) {
            setActive(over);
        }
        /* Didn't hit anything. */
        else {
            setActive(nullptr);
        }
    }

    void Editor::dragState(GPoint pt) {
        /* TODO: Collisions with other states? */
        if (hoverNode) {
            hoverNode->position(hoverNode->position() + (lastState - hoverNode->position()));
            lastState = pt;
            requestRepaint();
            dirty();
        }
    }

    void Editor::dragTransition(GPoint pt) {
        dragEdge1 = pt;

        setHover(mViewer->nodeAt(pt));
        requestRepaint();
    }

    void Editor::mouseDragged(double x, double y) {
        if (dragType == DragType::NODE) {
            dragState(mViewer->graphicsToWorld(GPoint{x, y}));
        } else if (dragType == DragType::EDGE) {
            dragTransition(mViewer->graphicsToWorld(GPoint{x, y}));
        }
    }

    void Editor::mouseReleased(double x, double y) {
        if (dragType == DragType::EDGE) {
            finishCreatingEdge(mViewer->graphicsToWorld(GPoint{ x, y }));
        }
        dragType = DragType::NONE;
        requestRepaint();
    }

    void Editor::finishCreatingEdge(GPoint pt) {
        /* For starters, see what we hit. */
        auto end = mViewer->nodeAt(pt);

        /* If this isn't a state, there's nothing to do. */
        if (end == nullptr) {
            edgeStart = nullptr;
            return;
        }

        /* If this is the same state, confirm that we dragged enough for this
         * to count as a self-loop.
         */
        if (end == edgeStart) {
            double theta0 = angleOf(dragEdge0 - edgeStart->position());
            double theta1 = angleOf(dragEdge1 - edgeStart->position());

            /* Each are in (-pi, pi], so the difference is in [-2pi, 2pi]. We want something in
             * (-pi, pi]. To do this, first get us in [0, 2pi].
             */
            double thetaDiff = theta0 - theta1;
            if (thetaDiff < 0) {
                thetaDiff = fmod(thetaDiff + 2 * M_PI, 2 * M_PI);
            }

            /* Now, shift us to [-pi, pi] from [0, 2pi]. */
            if (thetaDiff > M_PI) {
                thetaDiff -= 2 * M_PI;
            }

            if (fabs(thetaDiff) < kSelfTransitionThreshold) {
                edgeStart = nullptr;
                return;
            }
        }

        /* If the transition already exists, select it and do nothing. */
        auto* edge = mViewer->edgeBetween(edgeStart, end);
        if (!edge) {
            edge = mViewer->newEdge(edgeStart, end);
            dirty();
        }

        setActive(edge);
    }

    void Editor::deleteNode(Node* node) {
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

    void Editor::deleteEdge(Edge* edge) {
        /* Remove from the list of transitions. */
        mViewer->removeEdge(edge);

        if (activeEdge == edge) setActive(nullptr);
        if (hoverEdge  == edge) setHover(nullptr);
        dirty();
    }

    void Editor::dirty() {
        for (auto listener: mListeners) {
            listener->isDirty();
        }
    }

    void Editor::drawGraph(GCanvas* canvas) {
        /* Configure styles. */
        unordered_map<Node*, NodeStyle> stateStyles;
        unordered_map<Edge*, EdgeStyle> transitionStyles;

        /* Active and hover states are NOT mutually exclusive! */
        if (activeNode) {
            stateStyles[activeNode].fillColor = kActiveStateColor;
        }
        if (hoverNode) {
            stateStyles[hoverNode].borderColor = kHoverBorderColor;
            stateStyles[hoverNode].lineWidth   = kHoverBorderWidth;
            stateStyles[hoverNode].radius     -= kHoverBorderWidth / 2.0;
        }

        /* Active transition always takes precedence over hover transition. */
        if (hoverEdge) {
            transitionStyles[hoverEdge].color     = kHoverTransitionColor;
            transitionStyles[hoverEdge].lineWidth = GraphEditor::kEdgeTolerance;
        }
        if (activeEdge) {
            transitionStyles[activeEdge].color     = kActiveTransitionColor;
            transitionStyles[activeEdge].lineWidth = kActiveTransitionWidth;
        }

        mViewer->draw(canvas, stateStyles, transitionStyles);
    }

    void Editor::drawDraggedEdge(GCanvas* canvas) {
        if (dragType == DragType::EDGE) {
            mViewer->drawArrow(canvas, dragEdge0, dragEdge1, kNewTransitionWidth, kNewTransitionColor);
        }
    }

    void Editor::draw(GCanvas* canvas) {
        drawGraph(canvas);
        drawDraggedEdge(canvas);
    }

    void Editor::requestRepaint() {
        for (auto listener: mListeners) {
            listener->needsRepaint();
        }
    }

    void Editor::addListener(std::shared_ptr<Listener> listener) {
        mListeners.push_back(listener);
    }


    /**** Default listener interface. ****/
    void Listener::isDirty() {
        // Do nothing
    }

    void Listener::needsRepaint() {
        // Do nothing
    }

    void Listener::entityHovered(Entity *) {
        // Do nothing
    }

    void Listener::entitySelected(Entity *) {
        // Do nothing
    }
}