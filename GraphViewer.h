/******************************************************************************
 * Generic graph renderer. Without any fancy work on your part, this can render
 * graphs in an abstract space.
 */

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
    class ViewerBase;
    class Node;
    class Edge;

    class NodeArgs;
    class EdgeArgs;

    /* Aspect ratio for the viewer. This is exposed so that items can be positioned
     * appropriately in the logical space.
     */
    const double kDefaultAspectRatio = 5.0 / 3.0;

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

    /* Base type for graph entities. */
    class Entity {
    public:
        virtual ~Entity() = default;
    };

    /* A node in the graph. */
    class Node: public Entity {
    public:
        virtual ~Node() = default;

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
        void  position(const GPoint& pt);

        /* Draws the given node. */
        virtual void draw(ViewerBase* base,        // For converting units
                          GCanvas*    canvas,      // Where to draw
                          const NodeStyle& style); // Style

        /* Serialize auxiliary data to JSON. By default this just returns null,
         * but you can override this to customize the data stashed away.
         */
        virtual JSON toJSON();

    protected:
        /* This exists so that Node can be used directly. It completely ignores
         * the aux data. You shouldn't need to use this constructor.
         */
        Node(ViewerBase* viewer,
             const NodeArgs& args,
             JSON /* ignored */);

        Node(ViewerBase* viewer, const NodeArgs& args);

    private:
        ViewerBase* owner;

        /* Position in logical space. */
        GPoint mPos;

        std::size_t mIndex;

        std::string mLabel;

        friend class ViewerBase;
        template <typename N, typename T> friend class Viewer;
    };

    class Edge: public Entity {
    public:
        virtual ~Edge() = default;

        Node* from();
        Node* to();

        std::string label();
        void label(const std::string& label);

        /* Serialize auxiliary data to JSON. By default this just returns null,
         * but you can override this to customize the data stashed away.
         */
        virtual JSON toJSON();

    protected:
        /* This exists so that you can use Edge as a default type for the viewer.
         * You should not need to use this constructor directly.
         */
        Edge(ViewerBase* owner, const EdgeArgs& args,
             JSON /* ignored */);

        Edge(ViewerBase* owner, const EdgeArgs& args);

    private:
        ViewerBase* mOwner;

        /* Origin / endpoint. */
        Node* mFrom, *mTo;

        /* Label, if any. */
        std::string mLabel;

        /* Style of transition used. */
        std::shared_ptr<struct EdgeRender> style;

        friend class ViewerBase;
        template <typename N, typename T> friend class Viewer;
    };

    /* Base type containing the logic to draw a graph. You will likely not
     * need to make use of this type directly; instead, use the parameterized
     * Viewer type.
     */
    class ViewerBase {
    public:
        /* Aspect ratio. */
        double aspectRatio();
        void aspectRatio(double ratio);

        /* Serializes the viewer to a JSON object. */
        JSON toJSON();

        void setBounds(const GRectangle& bounds);

        /* Default graphics parameters will be used everywhere, except for the
         * explicit states and transitions that override it.
         */
        void draw(GCanvas* canvas,
                  const std::unordered_map<Node*, NodeStyle>& nodeStyles = {},
                  const std::unordered_map<Edge*, EdgeStyle>& edgeStyles = {});

        /* Rectangle we were instructed to fill. */
        GRectangle bounds() const;

        /* Rectangle computed to contain all the content. */
        GRectangle computedBounds() const;

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

        std::size_t numNodes();

        /* TODO: Hide all five of these; they're overridden in the derived type. */
        bool hasEdge(Node* from, Node* to);
        void forEachNode(std::function<void(Node*)>);
        void forEachEdge(std::function<void(Edge*)>);
        Node* nodeAt(const GPoint& pt);
        Edge* edgeAt(const GPoint& pt);

    protected:
        virtual JSON auxData();

    private:

        /* Geometry. */
        double baseX = 0, baseY = 0;
        double width = 0, height = 0;
        double mAspectRatio = kDefaultAspectRatio;

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

        JSON nodesToJSON();
        JSON edgesToJSON();

        JSON toJSON(Node* node);
        JSON toJSON(Edge* edge);

        Edge* edgeBetween(Node* from, Node* to);
        Node* nodeLabeled(const std::string& label);

        /* Graphics routines. */
        void drawTransition(GCanvas* canvas, std::shared_ptr<Edge> transition);

        void drawTransitionLabel(GCanvas* canvas, const GPoint& p0, const GPoint& p1,
                                 const std::string& label, bool hugLine);
        void drawArrowhead(GCanvas* canvas, const GPoint& from, const GPoint& to,
                           double thickness, const std::string& color);        

        /* Recalculates the renderer for each transition. */
        void calculateEdgeEndpoints();

        friend struct LineEdge;
        friend struct LoopEdge;
        friend class  Node;
        friend class  Edge;

        template <typename NodeType, typename EdgeType>
        friend class Viewer;
    };

    /* Viewer / editor for an underlying graph. This is parameterized on the types
     * of the nodes and edges so that you can substitute your own types in as needed.
     *
     * For this to work properly, the node and edge types must be subtypes of Node
     * and Edge.
     */
    template <typename N = Node, typename E = Edge>
    class Viewer: public ViewerBase {
    public:
        using NodeType = N;
        using EdgeType = E;

        /* Constructs a new, empty viewer. */
        Viewer() = default;

        /* Deserializes the viewer from the given JSON data. The JSON data is
         * presumed to have been written by a previous Viewer.
         *
         * This JSON object may have a field named "aux." If it does, that data
         * is purely for use by the subclasses and can be used however best
         * suits the viewer.
         */
        Viewer(JSON data);

        NodeType* newNode(const GPoint& location);
        void removeNode(NodeType* state);
        NodeType* nodeLabeled(const std::string& name);

        /* TODO: Semantics if the transition already exists? */
        EdgeType* newEdge(NodeType* from, NodeType* to, const std::string& label = "");
        void removeEdge(EdgeType* edge);

        /* What's at this position? */
        NodeType* nodeAt(const GPoint& pt);
        EdgeType* edgeAt(const GPoint& pt);

        void forEachNode(std::function<void(NodeType*)>);
        void forEachEdge(std::function<void(EdgeType*)>);

        EdgeType* edgeBetween(NodeType* from, NodeType* to);

    private:
        /* Full constructors. */
        std::shared_ptr<NodeType> newNode(const GPoint& pt, size_t index, const std::string& label, JSON aux);
        std::shared_ptr<EdgeType> newEdge(NodeType* from, NodeType* to, const std::string& label,   JSON aux);
    };



    /***** Implementation Below This Point *****/

    /* Node and edge constructor args. */
    class NodeArgs {
        GPoint pt;
        std::size_t index;
        std::string label;

        NodeArgs(const GPoint& pt, std::size_t index, const std::string& label) : pt(pt), index(index), label(label) {}

        friend class Node;
        template <typename, typename> friend class Viewer;
    };

    class EdgeArgs {
        Node* from;
        Node* to;
        std::string label;

        EdgeArgs(Node* from, Node* to, const std::string& label) : from(from), to(to), label(label) {}


        friend class Edge;
        template <typename, typename> friend class Viewer;
    };

    template <typename NodeType, typename EdgeType>
    NodeType* Viewer<NodeType, EdgeType>::newNode(const GPoint& pt) {
        /* Get the ID for this state. */
        size_t id = numNodes();
        if (!freeNodeIDs.empty()) {
            id = *freeNodeIDs.begin();
            freeNodeIDs.erase(freeNodeIDs.begin());
        }

        auto result = newNode(pt, id, "", nullptr);
        return result.get();
    }

    template <typename NodeType, typename EdgeType>
    std::shared_ptr<NodeType> Viewer<NodeType, EdgeType>::newNode(const GPoint& pt, size_t index, const std::string& label, JSON j) {
        auto result = std::shared_ptr<NodeType>(new NodeType(this, NodeArgs{pt, index, label}, j));
        nodes.insert(result);
        return result;
    }

    template <typename NodeType, typename EdgeType>
    EdgeType* Viewer<NodeType, EdgeType>::newEdge(NodeType* from, NodeType* to, const std::string& label) {
        return newEdge(from, to, label, nullptr).get();
    }

    template <typename NodeType, typename EdgeType>
    std::shared_ptr<EdgeType> Viewer<NodeType, EdgeType>::newEdge(NodeType* from, NodeType* to, const std::string& label, JSON aux) {
        auto edge = std::shared_ptr<EdgeType>(new EdgeType(this, EdgeArgs{from, to, label}, aux));
        edges[from][to] = edge;
        calculateEdgeEndpoints();
        return edge;
    }

    template <typename NodeType, typename EdgeType>
    NodeType* Viewer<NodeType, EdgeType>::nodeAt(const GPoint& pt) {
        return static_cast<NodeType*>(ViewerBase::nodeAt(pt));
    }

    template <typename NodeType, typename EdgeType>
    EdgeType* Viewer<NodeType, EdgeType>::edgeAt(const GPoint& pt) {
        return static_cast<EdgeType*>(ViewerBase::edgeAt(pt));
    }

    template <typename NodeType, typename EdgeType>
    void Viewer<NodeType, EdgeType>::removeNode(NodeType* node) {
        auto itr = find_if(nodes.begin(), nodes.end(), [&](std::shared_ptr<Node> n) {
             return n.get() == node;
        });
        if (itr == nodes.end()) return;

        nodes.erase(itr);

        /* Remove transitions from the state. */
        edges.erase(node);

        /* Remove transitions to the state. */
        for (auto& e1: edges) {
            auto itr = e1.second.begin();
            while (itr != e1.second.end()) {
                if (itr->first == node) {
                    itr = e1.second.erase(itr);
                } else {
                    ++itr;
                }
            }
        }

        freeNodeIDs.insert(node->index());

        calculateEdgeEndpoints();
    }

    template <typename NodeType, typename EdgeType>
    void Viewer<NodeType, EdgeType>::removeEdge(EdgeType* transition) {
        edges[transition->from()].erase(transition->to());
        calculateEdgeEndpoints();
    }

    template <typename NodeType, typename EdgeType>
    EdgeType* Viewer<NodeType, EdgeType>::edgeBetween(NodeType* from, NodeType* to) {
        return static_cast<EdgeType*>(ViewerBase::edgeBetween(from, to));
    }

    template <typename NodeType, typename EdgeType>
    NodeType* Viewer<NodeType, EdgeType>::nodeLabeled(const std::string& label) {
        return static_cast<NodeType*>(ViewerBase::nodeLabeled(label));
    }

    /* Deserialize. */
    template <typename NodeType, typename EdgeType>
    Viewer<NodeType, EdgeType>::Viewer(JSON j) {
        /* Decompress nodes. */
        std::size_t maxIndex = 0;
        std::map<std::size_t, Node*> byIndex;
        for (JSON jNode: j["nodes"]) {
            std::size_t index = jNode["index"].asInteger();
            std::string label = jNode["label"].asString();
            GPoint pos        = { jNode["pos"][0].asDouble(), jNode["pos"][1].asDouble() };

            auto node = newNode(pos, index, label, jNode["aux"]);

            byIndex[node->index()] = node.get();

            maxIndex = std::max(maxIndex, index);
        }

        /* Loop over nodes again, filling in missing node IDs. */
        for (size_t i = 0; i < maxIndex; i++) {
            freeNodeIDs.insert(i);
        }
        for (auto node: nodes) {
            freeNodeIDs.erase(node->index());
        }

        /* Decompress edges. */
        for (JSON jEdge: j["edges"]) {
            std::size_t from  = jEdge["from"].asInteger();
            std::size_t to    = jEdge["to"].asInteger();
            std::string label = jEdge["label"].asString();
            auto edge         = newEdge(static_cast<NodeType*>(byIndex.at(from)),
                                             static_cast<NodeType*>(byIndex.at(to)),
                                             label,
                                             jEdge["aux"]);
        }
    }

    template <typename NodeType, typename EdgeType>
    void Viewer<NodeType, EdgeType>::forEachNode(std::function<void(NodeType*)> callback) {
        return ViewerBase::forEachNode([&](Node* node) {
            return callback(static_cast<NodeType*>(node));
        });
    }

    template <typename NodeType, typename EdgeType>
    void Viewer<NodeType, EdgeType>::forEachEdge(std::function<void(EdgeType*)> callback) {
        return ViewerBase::forEachEdge([&](Edge* edge) {
            return callback(static_cast<EdgeType*>(edge));
        });
    }
}
