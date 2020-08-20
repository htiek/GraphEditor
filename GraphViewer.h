#pragma once

#include "gobjects.h"
#include "gwindow.h"
#include "Utilities/JSON.h"
#include <string>
#include <set>
#include <unordered_map>
#include <memory>
#include <functional>
#include <istream>

namespace GraphEditor {
    class Viewer;
    class Node;
    class Edge;

    /* Size of a node in the graphics system. This is exposed so that controllers can
     * determine how close something is to the center of a node (e.g. to determine
     * whether we're dragging a node vs. introducing a new node.
     */
    const double kNodeRadius = 0.035;

    /* Width of an edge. This is exposed so that controllers can draw lines representing
     * new edges.
     */
    const double kEdgeWidth = 3.0 / 1000; // 3px on a 1000px window;

    /* Width of a edge being hovered over. This is exposed so that controllers can tell
     * whether a edge is hovered.
     */
    const double kEdgeTolerance = 16.0 / 1000;

    /* Fonts used. Exposed so that other renderers can use the same fonts as us.
     * Ubuntu linux nails this with the defaults. Windows requires some hinting
     * to ensure that all the different characters show up. The fonts used on Windows
     * are (at least, according to Wikipedia) shipped with the OS and don't require any
     * custom installs.
     */
    #ifndef _WIN32
        const char* const kEdgeFont = "Monospace-18";
        const char* const kNodeFont = "Serif-ITALIC-18";
    #else
        const char* const kEdgeFont = "Lucida Sans Unicode-18";
        const char* const kNodeFont = "Times New Roman-ITALIC-18";
    #endif

    /* Default graphics parameters. */
    const double kNodeBorderWidth = 3.0 / 1000; // 3px on a 1000px window
    const std::string kNodeColor = "white";
    const std::string kNodeBorderColor = "black";
    const std::string kEdgeColor = "black";

    /* Styling for nodes. */
    struct NodeStyle {
        double radius           = kNodeRadius;
        double lineWidth        = kNodeBorderWidth;
        std::string fillColor   = kNodeColor;
        std::string borderColor = kNodeBorderColor;
    };

    /* Styling for transitions. */
    struct EdgeStyle {
        double lineWidth  = kEdgeWidth;
        std::string color = kEdgeColor;
    };


    /* Node renderer type. Responsible for drawing an individual node. */
    using NodeRenderer = std::function<void(Viewer*, GCanvas*, const NodeStyle& style)>;
    NodeRenderer defaultRendererFor(Node* node, bool drawLabel = true);

    /* Base type for graph entities. */
    class Entity {
    public:
        virtual ~Entity() = default;
    };

    /* A node in the graph. */
    class Node: public Entity {
    public:
        /* Each node is assigned a sequential number starting at 0. These numbers
         * count up and are recycled if nodes are deleted.
         */
        std::size_t index();

        /* Set/read label. By default, the label is empty. */
        const std::string& label();
        void label(const std::string& label);

        /* The state's position is given in world coordinates, where (0, 0) is the upper-left
         * corner of the content area and (1, 1 / kAspectRatio) is the lower-right corner.
         */
        const GPoint& position();
        void   position(const GPoint& pt);

        NodeRenderer renderer();
        void renderer(NodeRenderer renderer);

        std::shared_ptr<void> aux();
        void aux(std::shared_ptr<void> aux);

    private:
        Node(Viewer* viewer, const GPoint& pt, std::size_t index, const std::string& label);
        Viewer* owner;

        /* Position in logical space. */
        GPoint mPos;

        std::size_t mIndex;

        std::string mLabel;
        NodeRenderer mRenderer;

        std::shared_ptr<void> mAux;

        friend class Viewer;
    };

    class Edge: public Entity {
    public:
        Node* from();
        Node* to();

        std::string label();
        void label(const std::string& label);

        std::shared_ptr<void> aux();
        void aux(std::shared_ptr<void> aux);

    private:
        friend class Viewer;
        Viewer* mOwner;

        Edge(Viewer* owner, Node* from, Node* to, const std::string& label);

        /* Origin / endpoint. */
        Node* mFrom, *mTo;

        /* Label, if any. */
        std::string mLabel;

        std::shared_ptr<void> mAux;

        /* Style of transition used. */
        std::shared_ptr<struct EdgeRender> style;
    };

    /* Base type for auxiliary data associated with the viewer. If you associate
     * auxiliary data with the viewer, then
     *
     * 1. each new node will have auxiliary data automatically built for it;
     * 2. each new edge will have auxiliary data automatically build for it;
     * 3. when the viewer is serialized, the aux data per node/edge is too; and
     * 4. when the viewer is serialized, some additional aux data can be stored too.
     */
    class Aux {
    public:
        virtual ~Aux() = default;

        virtual std::shared_ptr<void> newNode(Node* node) = 0;
        virtual std::shared_ptr<void> newEdge(Edge* edge) = 0;

        virtual std::shared_ptr<void> readNodeAux(Node* node, JSON j) = 0;
        virtual std::shared_ptr<void> readEdgeAux(Edge* edge, JSON j) = 0;

        virtual JSON writeNodeAux(std::shared_ptr<void> aux) = 0;
        virtual JSON writeEdgeAux(std::shared_ptr<void> aux) = 0;

        virtual void readAux(JSON j) = 0;
        virtual JSON writeAux() = 0;

        Viewer* viewer();

    private:
        Viewer* mViewer;
        friend class Viewer;
    };

    /* View of an automaton that supports styling, highlighting, etc.
     *
     * Although this is called a "viewer," it does support editing of the underlying
     * graph via adding and removing nodes and edges and repositioning things. The
     * idea is that you can load a viewer from a file by populating it with a bunch
     * of new nodes and edges.
     */
    class Viewer {
    public:
        /* Constructs a viewer with the associated auxiliary data. */
        Viewer(std::shared_ptr<Aux> mAux = nullptr);

        /* Deserializes the viewer from the JSON source. If aux is non-null, aux will
         * be used to guide the deserialization.
         */
        Viewer(std::istream& in, std::shared_ptr<Aux> mAux = nullptr);

        /* Seriealizes the viewer to a JSON object. */
        JSON toJSON();

        void setBounds(const GRectangle& bounds);

        /* Default graphics parameters will be used everywhere, except for the
         * explicit states and transitions that override it.
         */
        void draw(GCanvas* canvas,
                  const std::unordered_map<Node*, NodeStyle>& stateStyles,
                  const std::unordered_map<Edge*, EdgeStyle>& transitionStyles);

        /* Rectangle we were instructed to fill. */
        GRectangle bounds() const;

        /* Rectangle computed to contain all the content. */
        GRectangle computedBounds() const;

        Node* newNode(const GPoint& location);
        void removeNode(Node* state);
        Node* nodeLabeled(const std::string& name);

        /* TODO: Semantics if the transition already exists? */
        Edge* newEdge(Node* from, Node* to, const std::string& label = "");
        void removeEdge(Edge* edge);

        /* What's at this position? */
        Node* nodeAt(GPoint pt);
        Edge* edgeAt(GPoint pt);

        std::size_t numNodes();
        void forEachNode(std::function<void(Node*)>);
        void forEachEdge(std::function<void(Edge*)>);

        bool hasEdge(Node* from, Node* to);
        Edge* edgeBetween(Node* from, Node* to);

        /* Coordinate changes. */
        double     graphicsToWorld(double width);
        GPoint     graphicsToWorld(GPoint pt);
        GRectangle graphicsToWorld(GRectangle r);
        double     worldToGraphics(double width);
        GPoint     worldToGraphics(GPoint pt);
        GRectangle worldToGraphics(GRectangle r);

        /* Draws an arrow in the world. This is exposed because some clients need the ability
         * to do this. All coordinates are in WORLD COORDINATES, not GRAPHICS COORDINATES.
         */
        void drawArrow(GCanvas* canvas, const GPoint& from, const GPoint& to,
                       double thickness, const std::string& color);

        /* Aux structure, if any. */
        std::shared_ptr<Aux> aux();

    private:
        /* Auxiliary data. */
        std::shared_ptr<Aux> mAux;

        /* Geometry. */
        double baseX = 0, baseY = 0;
        double width = 0, height = 0;

        /* Where we were told to draw. */
        GRectangle rawBounds;

        /* List of all nodes / edges. */
        std::set<std::shared_ptr<Node>> nodes;

        /* Transitions, encoded as node -> node -> edge info. */
        std::unordered_map<Node*, std::unordered_map<Node*, std::shared_ptr<Edge>>> edges;

        /* Available numbers for states; if empty, use size of states
         * as the next free one.
         */
        std::set<int> freeNodeIDs;

        /* Full constructor for states. */
        std::shared_ptr<Node> newNode(const GPoint& location, const std::string& name, bool isStart, bool isAccepting);

        /* Graphics routines. */
        void drawTransition(GCanvas* canvas, std::shared_ptr<Edge> transition);

        void drawTransitionLabel(GCanvas* canvas, const GPoint& p0, const GPoint& p1,
                                 const std::string& label, bool hugLine);
        void drawArrowhead(GCanvas* canvas, const GPoint& from, const GPoint& to,
                           double thickness, const std::string& color);

        /* Handles everything but aux data. */
        std::shared_ptr<Node> newNodeNoAux(const GPoint& pt, size_t index, const std::string& label);
        std::shared_ptr<Edge> newEdgeNoAux(Node* from, Node* to, const std::string& label);

        /* Recalculates the renderer for each transition. */
        void calculateEdgeEndpoints();

        /* Serialization / deserialization utilities. */
        JSON nodesToJSON();
        JSON edgesToJSON();
        JSON auxToJSON();

        JSON toJSON(Node* node);
        JSON toJSON(Edge* edge);

        friend struct LineEdge;
        friend struct LoopEdge;
        friend class  Node;
        friend class  Edge;
    };
}
