#include "GraphViewer.h"
#include "../GUI/MiniGUI.h"
#include "GVector.h"
#include "Utilities/Unicode.h"
#include <cmath>
#include <set>
#include <unordered_map>
#include <sstream>
#include <cctype>
using namespace std;

namespace GraphEditor {
    namespace {
        /* Useful, not required. */
        const string kNonbreakingSpace = toUTF8(0xA0);

        /* Intended aspect ratio. */
        const double kAspectRatio = 5.0 / 3.0;

        /* State graphics parameters. */
        const string kStateFontColor = "black";

        /* Transition graphics parameters. */
        const double kLoopTransitionRadius = GraphEditor::kNodeRadius * 0.75;

        /* Length of the invisible line on which to draw the contents of a loop transition. */
        const double kLoopLabelLength = 150 / 1000.0;

        /* Font and height for transitions. */
        const string kTransitionFontColor = GraphEditor::kEdgeColor;
        const double kTransitionTextHeight = 48.0 / 1000; // 24pt in 1000px window

        /* Amount to offset the label by relative to the transition. */
        const double kTransitionLabelYOffset = 8.0 / 1000;
        const double kLoopTransitionYOffset  = 30.0 / 1000;

        /* How much, in radians, to rotate the origin points of the states when shifting
         * start positions of transitions.
         */
        const double kAvoidanceRotation = -M_PI / 6;

        /* Arrowhead parameters. */
        const double kArrowheadRotation = M_PI / 8;
        const double kArrowheadSize     = 0.02;

        /* Parameters for avoiding collisions with self-loops. */
        const int kLowAngle = -5;
        const int kHighAngle = 355;
        const int kAngleStep = 10;
    }

    /* Transitions can be either line transitions or loop transitions. */
    struct EdgeRender {
        EdgeRender(Viewer* editor, Edge* transition): editor(editor), transition(transition) {}
        virtual ~EdgeRender() = default;

        virtual void draw(GCanvas* canvas, double thickness, const string& color) const = 0;
        virtual bool contains(const GPoint& pt) const = 0;

        Viewer* editor;
        Edge* transition;
    };

    /* Linear transition. */
    struct LineEdge: EdgeRender {
        LineEdge(Viewer* editor, Edge* transition, GPoint from, GPoint to) : EdgeRender(editor, transition), lineStart(from), lineEnd(to) {}

        void draw(GCanvas* canvas, double thickness, const std::string& color) const override;
        bool contains(const GPoint& pt) const override;

        GPoint lineStart, lineEnd;
    };

    /* Self-loop. */
    struct LoopEdge: EdgeRender {
        LoopEdge(Viewer* editor, Edge* transition, const GPoint& center, const GPoint& arrowPt) : EdgeRender(editor, transition), center(center), arrowPt(arrowPt) {}

        void draw(GCanvas* canvas, double thickness, const string& color) const override;
        bool contains(const GPoint& pt) const override;

        /* Transition is represented by a circle. Where is the center of that
         * circle?
         */
        GPoint center;

        /* Point where the arrowhead is drawn. */
        GPoint arrowPt;
    };

    Viewer::Viewer(std::shared_ptr<Aux> aux) : mAux(aux) {
        if (aux) aux->mViewer = this;
    }

    shared_ptr<Aux> Viewer::aux() {
        return mAux;
    }

    Node* Viewer::newNode(const GPoint& pt) {
        /* Get the ID for this state. */
        size_t id = numNodes();
        if (!freeNodeIDs.empty()) {
            id = *freeNodeIDs.begin();
            freeNodeIDs.erase(freeNodeIDs.begin());
        }

        auto result = newNodeNoAux(pt, id, "");

        if (mAux) result->aux(mAux->newNode(result.get()));

        return result.get();
    }

    shared_ptr<Node> Viewer::newNodeNoAux(const GPoint& pt, size_t index, const string& label) {
        auto result = shared_ptr<Node>(new Node(this, pt, index, label));
        result->renderer(defaultRendererFor(result.get()));
        nodes.insert(result);
        return result;
    }

    Edge* Viewer::newEdge(Node* from, Node* to, const string& label) {
        auto edge = newEdgeNoAux(from, to, label);

        if (mAux) edge->aux(mAux->newEdge(edge.get()));
        return edge.get();
    }

    shared_ptr<Edge> Viewer::newEdgeNoAux(Node* from, Node* to, const string& label) {
        auto edge = shared_ptr<Edge>(new Edge(this, from, to, label));
        edges[from][to] = edge;
        calculateEdgeEndpoints();
        return edge;
    }

    void Viewer::draw(GCanvas* canvas,
                      const unordered_map<Node*, NodeStyle>& stateStyles,
                      const unordered_map<Edge*, EdgeStyle>& transitionStyles) {
        /* TODO: This is for testing purposes. Please remove this. */
        canvas->setColor("red");
        canvas->drawRect(baseX, baseY, width, height);

        /* Existing transitions underdraw the states so we don't see the lines. */
        for (auto start: edges) {
            for (auto end: start.second) {
                /* We could have null entries; skip them. */
                /* TODO: Is this true? */
                if (end.second) {
                    auto style = transitionStyles.count(end.second.get()) ? transitionStyles.at(end.second.get()) : EdgeStyle();
                    end.second->style->draw(canvas, style.lineWidth, style.color);
                }
            }
        }

        /* States. */
        for (auto state: nodes) {
            auto style = stateStyles.count(state.get())? stateStyles.at(state.get()) : NodeStyle();

            state->renderer()(this, canvas, style);
        }
    }

    namespace {
        bool isCloseTo(GPoint p0, GPoint p1, double distance) {
            double dx = p0.getX() - p1.getX();
            double dy = p0.getY() - p1.getY();

            return dx * dx + dy * dy <= distance * distance;
        }
    }

    Node* Viewer::nodeAt(GPoint pt) {
        /* TODO: Do we need to do this in reverse so that we pick the
         * topmost state?
         */
        for (auto node: nodes) {
            if (isCloseTo(pt, node->position(), kNodeRadius)) {
                return node.get();
            }
        }

        return nullptr;
    }

    Edge* Viewer::edgeAt(GPoint pt) {
        for (const auto& one: edges) {
            for (const auto& two: one.second) {
                /* Get the transition itself. */
                auto edge = two.second;

                if (edge->style->contains(pt)) {
                    return edge.get();
                }
            }
        }

        return nullptr;
    }

    double Viewer::graphicsToWorld(double width) {
        return width / this->width;
    }
    GPoint Viewer::graphicsToWorld(GPoint in) {
        return { (in.getX() - baseX) / width, (in.getY() - baseY) / width };
    }
    GRectangle Viewer::graphicsToWorld(GRectangle in) {
        auto top = graphicsToWorld(GPoint{ in.getX(), in.getY() });
        auto bot = graphicsToWorld(GPoint{ in.getX() + in.getWidth(), in.getY() + in.getHeight() });
        return { top.getX(), top.getY(), bot.getX() - top.getX(), bot.getY() - top.getY() };
    }

    double Viewer::worldToGraphics(double width) {
        return width * this->width;
    }
    GPoint Viewer::worldToGraphics(GPoint in) {
        return { in.getX() * width + baseX, in.getY() * width + baseY };
    }
    GRectangle Viewer::worldToGraphics(GRectangle in) {
        auto top = worldToGraphics(GPoint{ in.getX(), in.getY() });
        auto bot = worldToGraphics(GPoint{ in.getX() + in.getWidth(), in.getY() + in.getHeight() });
        return { top.getX(), top.getY(), bot.getX() - top.getX(), bot.getY() - top.getY() };
    }

    /* All parameters are in world coordinates. */
    void Viewer::drawArrow(GCanvas* canvas, const GPoint& from, const GPoint& to,
                           double thickness, const string& color) {
        GLine line(worldToGraphics(from), worldToGraphics(to));
        line.setLineWidth(ceil(thickness * width));
        line.setColor(color);

        canvas->draw(&line);

        drawArrowhead(canvas, from, to, thickness, color);
    }

    void Viewer::drawArrowhead(GCanvas* canvas, const GPoint& from, const GPoint& to,
                               double thickness, const string& color) {
        /* Draw the arrowheads. First, get a vector pointing from end to start so that
         * we can shift it around to compute the endpoints.
         */
        GVector v = normalizationOf(from - to);

        /* Compute the two endpoints. */
        GPoint left  = to + rotate(v, kArrowheadRotation)  * kArrowheadSize;
        GPoint right = to + rotate(v, -kArrowheadRotation) * kArrowheadSize;

        GLine line(worldToGraphics(left), worldToGraphics(to));
        line.setLineWidth(ceil(thickness * width));
        line.setColor(color);
        canvas->draw(&line);

        line.setStartPoint(worldToGraphics(right));
        canvas->draw(&line);
    }

        namespace {
        /* Given a quadratic equation, returns whether there are any solutions that
         * correspond to a line/circle intersection. This happens if solutions exist
         * UNLESS both intersections are less than zero or both intersections are
         * greater than one.
         */
        size_t quadraticSolnsInRange(double a, double b, double c) {
            double discriminant = b * b - 4 * a * c;
            if (discriminant < 0) return 0;

            double x1 = (-b + sqrt(discriminant)) / (2 * a);
            double x2 = (-b - sqrt(discriminant)) / (2 * a);

            return !((x1 < 0 && x2 < 0) || (x1 > 1 && x2 > 1));
        }

        /* Counts collisions a circle and a collection of lines. */
        size_t collisionsBetween(const GPoint& center, double radius,
                                 const vector<pair<GPoint, GPoint>>& lines) {
            /* Any point (x, y) on a circle satisfies
             *
             *    (x - x_c)^2 + (y - y_c)^2 = r^2.
             *
             * Any point on the line from p0 to p1 has parametric form
             *
             *    (x(t), y(t)) = p0 + t(p1 - p0).
             *
             * Substituting, we get
             *
             *    (p0x + t(p1x - p0x) - x_c)^2 + (p0y + t(p1y - p0y) - y_c)^2 = r^2
             *
             * Everything here except for t is a constant. Isolating t and simplifying, we get
             *
             *    (t(p1x - p0x) + p0x - x_c)^2 + (t(p1y - p0y) + p0y - y_c)^2 = r^2
             *       --- dx --                     --- dy ---
             *
             *    (t * dx + p0x - x_c)^2 + (t * dy + p0y - y_c)^2 = r^2
             *              --- sx --                -- sy ---
             *
             *    (t * dx + sx)^2 + (t * dy + sy)^2 = r^2
             *
             *    ((dx)^2 t^2 + 2*dx*sx*t + (sx)^2) + ((dy)^2 t^2 + 2*dy*dy*t + (sy)^2) = r^2
             *
             *    ((dx)^2 + (dy)^2) t^2 + 2(dx*sx + dy*sy) t + ((sx)^2 + (sy)^2 - r^2) = 0
             *        dot(d, d) t^2 + 2 dot(d, s) t + (dot(s, s) - r^2) = 0
             *
             * At this point this is a quadratic, and we just need to count solutions.
             */
            size_t solns = 0;
            for (const auto& line: lines) {
                GVector d = line.second - line.first;
                GVector s = line.first - center;

                solns += quadraticSolnsInRange(dot(d, d), 2 * dot(d, s), dot(s, s) - radius * radius);
            }
            return solns;
        }

        size_t collisionsBetween(const GPoint&, double,
                                 const vector<pair<GPoint, double>>&) {
            /* TODO: Implement this function to count circle/circle collisions. */
            return 0;
        }

        /* Given a collection of circles and lines and a new circle, determines how many
         * collisions there are. This counts the number of colliding ENTITIES, not the
         * number of collision points overall.
         */
        size_t collisionsBetween(const GPoint& center, double radius,
                                 const vector<pair<GPoint, GPoint>>& lines,
                                 const vector<pair<GPoint, double>>& circles) {
            return collisionsBetween(center, radius, lines) +
                   collisionsBetween(center, radius, circles);
        }

        /* Determines the best angle at which to orient a self-loop, which is one that
         * hits the fewest other objects.
         */
        double bestThetaFor(const GPoint& stateCenter, const vector<pair<GPoint, GPoint>>& lines,
                            const vector<pair<GPoint, double>>& circles) {
            /* Our algorithm for placing the circle goes as follows. We iterate over a fixed
             * number of potential angles that we can use. For each one, we count the number
             * of collisions that would result if we put the circle there, forming an array
             * that you can think of as a "height map" of the collisions.
             *
             * We then find the minimum number of collisions and look for the longest range
             * in the array (remembering to loop back around when we're done!), which
             * corresponds to the widest margin of error we can find. From there, we then
             * pick the midpoint of that range.
             *
             * TODO: It would be a LOT more elegant to do this by using some sort of nice
             * and pretty math instead of trial and error. Can you improve upon this?
             */
            vector<size_t> collisions;
            for (int degAngle = kLowAngle; degAngle < kHighAngle; degAngle += kAngleStep) {
                double theta = degAngle * M_PI / 180;
                GPoint center = stateCenter + unitToward(theta) * kNodeRadius;

                collisions.push_back(collisionsBetween(center, kLoopTransitionRadius, lines, circles));
            }

            /* Find the minimum number of collisions. */
            size_t min = *min_element(collisions.begin(), collisions.end());

            /* Find the longest range of minimum values. */
            size_t bestStart  = 0;
            size_t bestLength = 0;

            size_t currStart  = 0;
            size_t currLength = 0;

            /* To handle wraparound, scan backwards over the array and look for the
             * first spot that isn't the min.
             */
            for (size_t i = collisions.size(); i > 0; --i) {
                if (collisions[i - 1] != min) break;

                /* Back up a step. */
                currStart = (currStart + collisions.size() - 1) % collisions.size();
                currLength++;
            }

            for (size_t i = 0; i < collisions.size(); i++) {
                /* Doesn't match? Gotta stop. */
                if (collisions[i] != min) {
                    if (bestLength < currLength) {
                        bestLength = currLength;
                        bestStart  = currStart;
                    }
                    currLength = 0;
                    currStart  = i+1;
                }
                /* Otherwise, extend. */
                else {
                    currLength++;
                }
            }

            /* Handle edge case of finding the best at the end. */
            if (bestLength < currLength) {
                bestLength = currLength;
                bestStart  = currStart;
            }

            /* Casts to double necessary here to avoid bizarre integer overflows as negative
             * angles turn into massively positive unsigned angles!
             */
            double lowTheta   = (kLowAngle + double(bestStart * kAngleStep)) * M_PI / 180;
            double highTheta  = (kLowAngle + double(bestStart + bestLength - 1) * kAngleStep) * M_PI / 180;
            return (lowTheta + highTheta) / 2;
        }

        /* Given the center of a state and the point at which the loop is centered, returns
         * a point where they intersect - a place where the arrow can be drawn.
         */
        GPoint loopArrowPointFor(const GPoint& stateCenter, const GPoint& loopCenter) {
            /* Trig time! We have two circles where one is centered on the border of
             * another. We want to then find one of the intersection points. How do we
             * do it?
             *
             * For now, imagine that everything is colinear, like this:
             *
             *                *
             *               / \
             *           r  /   \ r'
             *             /  r  \
             *            * ----- *
             *          state      loop
             *         center     center
             *
             * We want to know the angle theta that is made between the state center,
             * the loop center, and the third triangle point (their intersection). The
             * Law of Cosines tells us that
             *
             *       r'^2 = r^2 + r^2 - 2r^2 cos theta
             *
             * Which, after some rearranging, gives us
             *
             *       theta = arccos(1 - r'^2 / 2r^2)
             *
             * Use this to get that angle measure.
             */
            double theta = acos(1 - kLoopTransitionRadius * kLoopTransitionRadius / (2 * kNodeRadius * kNodeRadius));

            /* Rotate the vector from the state to loop center by this amount. */
            return stateCenter + rotate(loopCenter - stateCenter, theta);
        }

        /* Boundaries of the world, represented as lines. */
        vector<pair<GPoint, GPoint>> worldBoundaries() {
            const double lft = 0;
            const double rgt = 1;
            const double top = 0;
            const double bot = 1 / kAspectRatio;

            return {
                { { lft, top }, { rgt, top } },
                { { lft, bot }, { rgt, bot } },
                { { lft, top }, { lft, bot } },
                { { rgt, top }, { rgt, bot } },
            };
        }
    }

    /* Determines where each transition should start and end. There are dependencies
     * across these transitions, so we need to do this all at once.
     */
    void Viewer::calculateEdgeEndpoints() {
        /* List of all line segments used. */
        vector<pair<GPoint, GPoint>> lines = worldBoundaries();

        /* First, handle linear transitions. */
        forEachEdge([&](Edge* transition) {
            if (transition->from() != transition->to()) {
                /* Center coordinates. */
                GPoint p0 = transition->from()->position();
                GPoint p1 = transition->to()->position();

                /* If there is a transition running in the reverse direction, we need to shift
                 * this transition over so that we don't overlap it.
                 */
                if (hasEdge(transition->to(), transition->from())) {
                    /* Unit vector pointing in the p0 -> p1 direction saying how much we need to rotate. */
                    auto p0Delta = rotate(normalizationOf(p1 - p0), kAvoidanceRotation);

                    /* Unit vector pointing in the p1 -> p0 direction saying how much we need to rotate. */
                    auto p1Delta = rotate(normalizationOf(p0 - p1), -kAvoidanceRotation);

                    /* At the borders. */
                    p0 += p0Delta * kNodeRadius;
                    p1 += p1Delta * kNodeRadius;
                }
                /* Otherwise, translate the center points to the borders. */
                else {
                    p0 += normalizationOf(p1 - p0) * kNodeRadius;
                    p1 += normalizationOf(p0 - p1) * kNodeRadius;
                }

                transition->style = make_shared<LineEdge>(this, transition, p0, p1);
                lines.push_back(make_pair(p0, p1));
            }
        });

        /* All placed circles. Initially, that's all the states. */
        vector<pair<GPoint, double>> circles;
        for (auto state: nodes) {
            circles.push_back(make_pair(state->position(), kNodeRadius));
        }

        /* Now, place all self-loops. */
        forEachEdge([&](Edge* transition) {
            if (transition->from() == transition->to()) {
                double theta = bestThetaFor(transition->from()->position(), lines, circles);

                GPoint center  = transition->from()->position() + unitToward(theta) * kNodeRadius;
                GPoint arrowPt = loopArrowPointFor(transition->from()->position(), center);

                transition->style = make_shared<LoopEdge>(this, transition, center, arrowPt);
                circles.push_back(make_pair(center, kNodeRadius));
            }
        });
    }

    /* Linear transition implementation. */
    bool LineEdge::contains(const GPoint& pt) const {
        /* Our goal is to see both (1) how far from the line we are and (2) how far
         * down the line we are.
         *
         * To do this, we're going to change coordinates. Let b1 be a vector pointing
         * 90 degrees to the right of the line, and let b2 be a vector pointing down
         * the line. That gives us this coordinate system:
         *
         *              b2
         *              ^
         *              |
         *              |
         *          ----+----> b1
         *
         * Now, we rewrite our cursor's position in this coordinate system. We do this
         * by multiplying the cursor vector by a rotation matrix [b1, b2].
         *
         * From here we can see where we are. To be "close enough," we'll say that the
         * cursor needs to have x coordinate that places it within the hover width,
         * and it needs to have a y coordinate between zero and the length of the line.
         */

        /* Switch coordinate systems to place the origin of the transition at (0, 0). */
        GVector cursor = pt - lineStart;

        /* Get the vectors b1 and b2. */
        GVector lineVec = lineEnd - lineStart;

        GVector b2 = lineVec / magnitudeOf(lineVec);
        GVector b1 = rotate(b2, M_PI / 2);

        /* Compute the cursor's representation in this system. That's
         *
         * | b1x b2x | |cx|
         * | b1y b2y | |cy|
         */
        GVector result = GMatrix(b1, b2) * cursor;

        /* Make sure we're in range. */
        return fabs(result.x) <= kEdgeTolerance / 2.0 &&
               result.y >= 0 && result.y <= magnitudeOf(lineVec);
    }

    namespace {
        /* Is this a space character? */
        bool isSpace(char32_t ch) {
            return ch >= 0 && ch <= 127 && isspace(ch);
        }

        /* Given a string, replaces all the spaces in the string with nonbreaking spaces. */
        string toNonbreakingSpaces(const string& input) {
            string result;
            for (char32_t ch: utf8Reader(input)) {
                if (isSpace(ch)) {
                    result += kNonbreakingSpace;
                } else {
                    result += toUTF8(ch);
                }
            }

            return result;
        }
    }

    void Viewer::drawTransitionLabel(GCanvas* canvas,
                                     const GPoint& p0, const GPoint& p1,
                                     const string& labelText,
                                     bool hugLine) {
        GPoint from = worldToGraphics(p0);
        GPoint to   = worldToGraphics(p1);

        string label = toNonbreakingSpaces(labelText);

        /* Determine the length of this line. */
        double length = magnitudeOf(to - from);

        /* Determine what font we should use for the label by computing a text render
         * and extracting the font it uses.
         */
        string font = TextRender::construct(label, {0, 0, length, width * kTransitionTextHeight }, kTransitionFontColor, kEdgeFont)->computedFont();

        /* Create a graphics object for the label. */
        GText text(label);
        text.setFont(font);
        text.setColor(kTransitionFontColor);

        /* Figure out where the label needs to go. */
        double theta = angleOf(to - from);

        /* Never draw text upside-down. See whether we go above or below the line. */
        if (theta < -M_PI / 2 || theta > M_PI / 2) {
            /* Below the line. Rotate our angle by 180 degrees and exchange the roles
             * of the endpoints.
             */
            theta += M_PI;
            swap(to, from);

            /* If we are supposed to hug the line, we need to do an extra step and shift the
             * line position over so that when we draw on top of it, we appear to have just
             * flipped rather than flipped and translated.
             */
            if (hugLine) {
                /* Get a perpendicular to the line. */
                GVector normal = rotate(normalizationOf(to - from), M_PI / 2) * text.getHeight();
                from += normal;
                to   += normal;
            }
        }

        /* We'll aim to draw on the transition as the baseline,
         * so we need to compute the (x, y) coordinate of the transition center.
         *
         * ... except that it's not the exact center. Rather, it's the center of the
         * transition, offset by half the width of the label. In other words, we want
         * to walk to the center, then advance a bit further
         */
        GPoint target = from + normalizationOf(to - from) * (length - text.getWidth()) / 2.0;

        /* Now, shift up off the line. */
        target += rotate(normalizationOf(to - from), -M_PI / 2) * ceil(width * kTransitionLabelYOffset);

        /* GText behaves strangely when rotated. The rotation is done around
         * the graphics origin point (0, 0) rather than the center of the object.
         * This means that we need to reposition the label so that it rotates the
         * text into the exact position we want.
         *
         * Goal: Given a target position of (x, y), find a position
         * (x', y') such that (x', y') rotates onto (x, y).
         *
         * Given a rotation angle theta, the ACTUAL position where the object
         * will be located is ROT(theta) (x', y').
         *
         * So we want ROT(theta) (x', y') = (x, y), meaning that
         * (x', y') = ROT(-theta)(x, y).
         */
        GPoint textPos = rotation(-theta) * target;

        /* GText rotations are in degrees. */
        text.rotate(theta * 180 / M_PI);
        text.setLocation(textPos);
        canvas->draw(&text);
    }

    void LineEdge::draw(GCanvas* canvas, double thickness, const string& color) const {
        editor->drawArrow(canvas, lineStart, lineEnd, thickness, color);
        editor->drawTransitionLabel(canvas, lineStart, lineEnd, transition->label(), false);
    }

    bool LoopEdge::contains(const GPoint& pt) const {
        /* We hit the circle if our distance to the center is within kHover of the
         * actual radius.
         */
        return fabs(magnitudeOf(pt - center) - kLoopTransitionRadius) < kEdgeTolerance;
    }

    void LoopEdge::draw(GCanvas* canvas, double width, const string& color) const {
        double size = 2 * editor->width * kLoopTransitionRadius;
        GPoint pt = editor->worldToGraphics(center);

        GOval toDraw(pt.getX() - size / 2, pt.getY() - size / 2, size, size);
        toDraw.setColor(color);
        toDraw.setLineWidth(ceil(editor->width * width));
        canvas->draw(&toDraw);

        /* Draw the arrowhead. You might think that we'd want the arrowhead
         * to appear as though it was entering the state normal to the circle
         * at the intersection point, but, surprisingly, that doesn't look good.
         * Instead, it's better to look like you're hitting the the circle
         * tangent to the line drawn between the state center and the loop
         * center.
         */
        GPoint exterior = arrowPt + (center - transition->from()->position());
        editor->drawArrowhead(canvas, exterior, arrowPt, width, color);


        /* We will draw the transition contents by imagining there's an invisible tangent
         * line to the circle that we'll draw on top of.
         */

        /* Get a vector pointing away from the circle center. */
        GVector out = normalizationOf(center - transition->from()->position());

        /* Move outward to the end of the loop. */
        GPoint tangentPoint = center + out * (kLoopTransitionRadius + kLoopTransitionYOffset);

        /* Construct a perpendicular vector and use it to form a line. */
        GVector tangent = rotate(out, M_PI / 2);
        GPoint p0 = tangentPoint + tangent * kLoopLabelLength / 2;
        GPoint p1 = tangentPoint - tangent * kLoopLabelLength / 2;
        editor->drawTransitionLabel(canvas, p0, p1, transition->label(), true);
    }

    void Viewer::forEachNode(function<void (Node *)> callback) {
        for (const auto& state: nodes) {
            callback(state.get());
        }
    }

    void Viewer::forEachEdge(function<void (Edge *)> callback) {
        for (const auto& e1: edges) {
            for (const auto& e2: e1.second) {
                callback(e2.second.get());
            }
        }
    }

    bool Viewer::hasEdge(Node* from, Node* to) {
        return edges.count(from) && edges[from].count(to);
    }

    void Viewer::removeNode(Node* node) {
        auto itr = find_if(nodes.begin(), nodes.end(), [&](shared_ptr<Node> n) {
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

    void Viewer::removeEdge(Edge* transition) {
        edges[transition->from()].erase(transition->to());
        calculateEdgeEndpoints();
    }

    GRectangle Viewer::bounds() const {
        return rawBounds;
    }

    GRectangle Viewer::computedBounds() const {
        return { baseX, baseY, width, height };
    }

    Edge* Viewer::edgeBetween(Node* from, Node* to) {
        if (!edges.count(from) || !edges.at(from).count(to)) return nullptr;
        return edges.at(from).at(to).get();
    }

    size_t Viewer::numNodes() {
        return nodes.size();
    }

    void Viewer::setBounds(const GRectangle& bounds) {
        rawBounds = bounds;

        /* Too narrow? */
        if (bounds.getWidth() / bounds.getHeight() <= kAspectRatio) {
            width = bounds.getWidth();
            height = width / kAspectRatio;
        } else {
            height = bounds.getHeight();
            width = height * kAspectRatio;
        }

        baseX = bounds.getX() + (bounds.getWidth()  - width)  / 2.0;
        baseY = bounds.getY() + (bounds.getHeight() - height) / 2.0;
    }

    Node::Node(Viewer* editor, const GPoint& pt, std::size_t id, const std::string& label)
        : owner(editor), mPos(pt), mIndex(id), mLabel(label) {
        owner->calculateEdgeEndpoints();
    }

    const string& Node::label() {
        return mLabel;
    }
    void Node::label(const string& label) {
        mLabel = label;
    }

    const GPoint& Node::position() {
        return mPos;
    }

    size_t Node::index() {
        return mIndex;
    }

    void Node::position(const GPoint& pt) {
        /* Clamp to appropriate bounds. */
        double x = pt.getX();
        if (x < kNodeRadius) x = kNodeRadius;
        if (x > 1 - kNodeRadius) x = 1 - kNodeRadius;

        double y = pt.getY();
        if (y < kNodeRadius) y = kNodeRadius;
        if (y > 1 / kAspectRatio - kNodeRadius) y = 1 / kAspectRatio - kNodeRadius;

        mPos = { x, y };
        owner->calculateEdgeEndpoints();
    }

    NodeRenderer Node::renderer() {
        return mRenderer;
    }

    void Node::renderer(NodeRenderer renderer) {
        mRenderer = renderer;
    }

    shared_ptr<void> Node::aux() {
        return mAux;
    }

    void Node::aux(shared_ptr<void> aux) {
        mAux = aux;
    }

    Node* Viewer::nodeLabeled(const string& label) {
        for (auto state: nodes) {
            if (state->label() == label) return state.get();
        }

        return nullptr;
    }

    NodeRenderer defaultRendererFor(Node* node, bool drawLabel) {
        return [=](Viewer* editor, GCanvas* canvas, const NodeStyle& style) {
            /* Calculate the size of the state. */
            double size = 2.0 * style.radius;
            auto bounds = editor->worldToGraphics({ node->position().getX() - size / 2.0, node->position().getY() - size / 2.0, size, size });

            GOval mainState(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight());

            mainState.setFilled(true);
            mainState.setFillColor(style.fillColor);
            mainState.setLineWidth(ceil(editor->worldToGraphics(style.lineWidth)));
            mainState.setColor(style.borderColor);
            canvas->draw(&mainState);

            if (drawLabel) {
                /* Draw the state name. */
                auto render = TextRender::construct(node->label(), bounds, kStateFontColor, kNodeFont);
                render->alignCenterVertically();
                render->alignCenterHorizontally();
                render->draw(canvas);
            }
        };
    }

    Edge::Edge(Viewer* owner, Node* from, Node* to, const std::string& label)
        : mOwner(owner), mFrom(from), mTo(to), mLabel(label) {
        owner->calculateEdgeEndpoints();
    }

    Node* Edge::to() {
        return mTo;
    }

    Node* Edge::from() {
        return mFrom;
    }

    string Edge::label() {
        return mLabel;
    }

    void Edge::label(const string& label) {
        mLabel = label;
    }

    shared_ptr<void> Edge::aux() {
        return mAux;
    }

    void Edge::aux(shared_ptr<void> aux) {
        mAux = aux;
    }

    /*** Aux functions ***/
    Viewer* Aux::viewer() {
        return mViewer;
    }

    /*** Serialization / Deserialization ***/

    /* JSON format is
     *
     * {"nodes", [<node data>],
     *  "edges", [<edge data>],
     *  "aux",   <aux data>}
     *
     * Here, each node is encoded as
     *
     *   { "index": <index>, "label": <label>, "pos": [<x>, <y>], "aux": <aux> }
     *
     * Each edge is encoded as
     *
     *   { "from": <index>, "to": <index>, "label": <label> }
     */

    JSON Viewer::nodesToJSON() {
        vector<JSON> result;
        for (auto node: nodes) {
            result.push_back(toJSON(node.get()));
        }
        return result;
    }

    JSON Viewer::toJSON(Node* node) {
        return JSON::object({
            { "index", node->index() },
            { "label", node->label() },
            { "pos",   JSON::array(node->position().getX(), node->position().getY()) },
            { "aux",   mAux? mAux->writeNodeAux(node->aux()) : nullptr }
        });
    }

    JSON Viewer::edgesToJSON() {
        vector<JSON> result;
        forEachEdge([&](Edge* edge) {
            result.push_back(toJSON(edge));
        });
        return result;
    }

    JSON Viewer::auxToJSON() {
        return mAux? mAux->writeAux() : nullptr;
    }

    JSON Viewer::toJSON(Edge* edge) {
        return JSON::object({
            { "from",  edge->from()->index()                         },
            { "to",    edge->to()->index()                           },
            { "label", edge->label()                                 },
            { "aux",   mAux? mAux->writeEdgeAux(edge->aux()) : nullptr }
        });
    }

    /* Serializes to JSON. */
    JSON Viewer::toJSON() {
        /* Pair that with the serialized NFA. */
        return JSON::object({
            { "nodes", nodesToJSON() },
            { "edges", edgesToJSON() },
            { "aux",   auxToJSON()   }
        });
    }

    /* Deserialize. */
    Viewer::Viewer(istream& in, std::shared_ptr<Aux> aux) : Viewer(aux) {
        JSON j = JSON::parse(in);

        /* Read aux data, if any. */
        if (aux) aux->readAux(j["aux"]);

        /* Decompress nodes. */
        size_t maxIndex = 0;
        map<size_t, Node*> byIndex;
        for (JSON jNode: j["nodes"]) {
            size_t index = jNode["index"].asInteger();
            string label = jNode["label"].asString();
            GPoint pos   = { jNode["pos"][0].asDouble(), jNode["pos"][1].asDouble() };

            auto node = newNodeNoAux(pos, index, label);
            if (aux) node->aux(aux->readNodeAux(node.get(), jNode["aux"]));

            byIndex[node->index()] = node.get();

            maxIndex = max(maxIndex, index);
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
            size_t from  = jEdge["from"].asInteger();
            size_t to    = jEdge["to"].asInteger();
            string label = jEdge["label"].asString();
            auto edge = newEdgeNoAux(byIndex.at(from), byIndex.at(to), label);
            if (aux) edge->aux(aux->readEdgeAux(edge.get(), jEdge["aux"]));
        }
    }
}
