#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

using namespace std;

const int COLOR_NONE = 0;
const int COLOR_GREEN = 1;
const int COLOR_RED = 2;
const int COLOR_BLUE = 3;

const int MODE_READY = 0;
const int MODE_RUNNING = 1;
const int MODE_DONE = 2;

const int ALGO_MRESTRINGIDO = 1;
const int ALGO_MRESTRICTIVO = 2;

const float MAX_WINDOW_PX = 800.0f;
const float COORDINATE_PLANE_SIZE = 100.0f;

float SCALE_FACTOR = MAX_WINDOW_PX / COORDINATE_PLANE_SIZE;
float NODE_RADIUS = SCALE_FACTOR * 0.8f;
const int CIRCLE_SEGMENTS = 12;

struct Point {
    float x, y;
    Point() { x = 0; y = 0; }
    Point(float _x, float _y) { x = _x; y = _y; }
};

class Edge {
public:
    int to;
    Edge(int t) { to = t; }
};

class Node {
public:
    int index;
    Point position;
    int color;
    vector<Edge> edges;

    Node(int i, Point pos) {
        index = i;
        position = pos;
        color = COLOR_NONE;
    }

    void addEdge(int to) {
        edges.push_back(Edge(to));
    }

    bool hasEdgeTo(int target) {
        for (auto& e : edges) {
            if (e.to == target) return true;
        }
        return false;
    }
};

class Graph {
private:
    int numVertices;
    vector<Node> nodes;
public:
    Graph(int vertices) {
        numVertices = vertices;
        nodes.reserve(vertices);
    }
    void addNode(int index, Point position) {
        nodes.push_back(Node(index, position));
    }
    void addEdge(int u, int v) {
        if (!nodes[u].hasEdgeTo(v)) {
            nodes[u].addEdge(v);
            nodes[v].addEdge(u);
        }
    }
    Node& getNode(int index) { return nodes[index]; }
    int getNumVertices() const { return numVertices; }
    void resetColors() {
        for (auto& n : nodes) n.color = COLOR_NONE;
    }
};

class GraphGenerator {
private:
    static float distanceEuclidean(Point a, Point b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return sqrt(dx * dx + dy * dy);
    }

public:
    static Graph createRandomGraph(int numNodes, int targetDegree) {
        if (numNodes <= 0) numNodes = 1;
        Graph g(numNodes);

        for (int i = 0; i < numNodes; i++) {
            float rx, ry;
            bool validPos;
            int attempts = 0;

            do {
                validPos = true;
                rx = static_cast<float>(rand() % 100);
                ry = static_cast<float>(rand() % 100);
                for (int j = 0; j < i; j++) {
                    if (distanceEuclidean(Point(rx, ry), g.getNode(j).position) < 3.0f) {
                        validPos = false;
                        break;
                    }
                }
                attempts++;
            } while (!validPos && attempts < 50);

            g.addNode(i, Point(rx, ry));
        }

        for (int i = 1; i < numNodes; i++) {
            int bestTarget = -1;
            float minDistance = 999999.0f;

            for (int j = 0; j < i; j++) {
                float dist = distanceEuclidean(g.getNode(i).position, g.getNode(j).position);
                if (dist < minDistance) {
                    minDistance = dist;
                    bestTarget = j;
                }
            }
            if (bestTarget != -1) {
                g.addEdge(i, bestTarget);
            }
        }

        for (int i = 0; i < numNodes; i++) {
            Node& current = g.getNode(i);

            while (current.edges.size() < targetDegree) {
                int bestTarget = -1;
                float minDistance = 999999.0f;

                for (int j = 0; j < numNodes; j++) {
                    if (i == j) continue;

                    Node& candidate = g.getNode(j);
                    if (candidate.edges.size() < targetDegree && !current.hasEdgeTo(j)) {
                        float dist = distanceEuclidean(current.position, candidate.position);
                        if (dist < minDistance) {
                            minDistance = dist;
                            bestTarget = j;
                        }
                    }
                }

                if (bestTarget != -1) {
                    g.addEdge(i, bestTarget);
                }
                else {
                    break;
                }
            }
        }
        return g;
    }
};

class Renderer {
public:
    static void setGLColor(int colorCode) {
        if (colorCode == COLOR_NONE) glColor3f(0.85f, 0.85f, 0.85f);
        else if (colorCode == COLOR_GREEN) glColor3f(0.2f, 0.85f, 0.2f);
        else if (colorCode == COLOR_RED) glColor3f(0.9f, 0.2f, 0.2f);
        else if (colorCode == COLOR_BLUE) glColor3f(0.2f, 0.4f, 0.9f);
    }

    static void drawCircle(float cx, float cy, float radius) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
            float angle = (2.0f * 3.14159265f * i) / CIRCLE_SEGMENTS;
            glVertex2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
        }
        glEnd();
    }

    static void init() {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, MAX_WINDOW_PX, MAX_WINDOW_PX, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    static void drawGraph(Graph& g) {
        glColor3f(0.7f, 0.7f, 0.7f);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        for (int i = 0; i < g.getNumVertices(); i++) {
            Point p1 = g.getNode(i).position;
            for (auto& edge : g.getNode(i).edges) {
                if (edge.to > i) {
                    Point p2 = g.getNode(edge.to).position;
                    glVertex2f(p1.x * SCALE_FACTOR, p1.y * SCALE_FACTOR);
                    glVertex2f(p2.x * SCALE_FACTOR, p2.y * SCALE_FACTOR);
                }
            }
        }
        glEnd();

        for (int i = 0; i < g.getNumVertices(); i++) {
            Node& n = g.getNode(i);
            setGLColor(n.color);
            drawCircle(n.position.x * SCALE_FACTOR, n.position.y * SCALE_FACTOR, NODE_RADIUS);

            if (n.color != COLOR_NONE) {
                glColor3f(0.0f, 0.0f, 0.0f);
                glBegin(GL_LINE_LOOP);
                for (int j = 0; j <= CIRCLE_SEGMENTS; j++) {
                    float angle = (2.0f * 3.14159265f * j) / CIRCLE_SEGMENTS;
                    glVertex2f(n.position.x * SCALE_FACTOR + NODE_RADIUS * cosf(angle),
                        n.position.y * SCALE_FACTOR + NODE_RADIUS * sinf(angle));
                }
                glEnd();
            }
        }
    }
};

struct DFSState {
    int nodeIndex;
    int colorTrying;
};

class IterativeSolver {
public:
    Graph* g;
    int heuristic;
    int backtrackCount;
    int totalSteps;
    const int MAX_STEPS = 1000000;
    bool done;
    bool success;
    vector<DFSState> stack;

    IterativeSolver(Graph* graph, int heur) : g(graph), heuristic(heur), backtrackCount(0), totalSteps(0), done(false), success(false) {
        int startNode = selectNextNode();
        if (startNode != -1) {
            stack.push_back({ startNode, 1 });
        }
        else {
            done = true;
            success = true;
        }
    }

    int selectNextNode() {
        int bestNode = -1;
        int bestScore = (heuristic == ALGO_MRESTRINGIDO) ? 999 : -1;

        for (int i = 0; i < g->getNumVertices(); i++) {
            if (g->getNode(i).color != COLOR_NONE) continue;

            if (heuristic == ALGO_MRESTRINGIDO) {
                // Variable mas restringida
                bool usedColors[4] = { false };
                for (auto& edge : g->getNode(i).edges) {
                    int neighborColor = g->getNode(edge.to).color;
                    if (neighborColor >= 1 && neighborColor <= 3) {
                        usedColors[neighborColor] = true;
                    }
                }
                int available = 0;
                for (int c = 1; c <= 3; c++) if (!usedColors[c]) available++;

                if (available < bestScore) {
                    bestScore = available;
                    bestNode = i;
                }
            }
            else if (heuristic == ALGO_MRESTRICTIVO) {
                // Variable mas restrictiva
                int uncoloredNeighbors = 0;
                for (auto& edge : g->getNode(i).edges) {
                    if (g->getNode(edge.to).color == COLOR_NONE) uncoloredNeighbors++;
                }

                if (uncoloredNeighbors > bestScore) {
                    bestScore = uncoloredNeighbors;
                    bestNode = i;
                }
            }
        }
        return bestNode;
    }

    void step() {
        if (done) return;
        totalSteps++;

        if (totalSteps > MAX_STEPS) {
            done = true;
            success = false;
            return;
        }

        if (stack.empty()) {
            done = true;
            success = false; 
            return;
        }

        DFSState& current = stack.back();

        if (current.colorTrying > 3 || (stack.size() == 1 && current.colorTrying > 1)) {
            g->getNode(current.nodeIndex).color = COLOR_NONE;
            backtrackCount++;
            stack.pop_back();
            if (!stack.empty()) {
                stack.back().colorTrying++;
            }
            return;
        }

        int c = current.colorTrying;
        bool isValid = true;
        for (auto& edge : g->getNode(current.nodeIndex).edges) {
            if (g->getNode(edge.to).color == c) {
                isValid = false;
                break;
            }
        }

        if (isValid) {
            g->getNode(current.nodeIndex).color = c;
            int nextNode = selectNextNode();

            if (nextNode == -1) {
                done = true;
                success = true;
            }
            else {
                stack.push_back({ nextNode, 1 });
            }
        }
        else {
            current.colorTrying++;
        }
    }
};

Graph* globalGraph = nullptr;
IterativeSolver* solver = nullptr;

int interactionMode = MODE_READY;
int selectedAlgorithm = ALGO_MRESTRINGIDO;
int userNodes = 50;
int userNeighbors = 3;

void printConsoleHUD() {
    cout << "\r                                                                                                                        \r";
    cout << "[MODE] ";
    if (interactionMode == MODE_READY) cout << "ENTER: Start Alg | 1/2: Select Alg | C: Clear | R: Reset Graph | ";
    else if (interactionMode == MODE_RUNNING) cout << "Calculating... | ";
    else if (interactionMode == MODE_DONE) cout << "Done | C: Clear | R: Reset Graph | ";

    cout << "[ALGO] ";
    if (selectedAlgorithm == ALGO_MRESTRINGIDO) cout << "Variable Mas Restringida";
    else if (selectedAlgorithm == ALGO_MRESTRICTIVO) cout << "Variable Mas Restrictiva";

    cout << "          " << flush;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_1 && interactionMode != MODE_RUNNING) {
        selectedAlgorithm = ALGO_MRESTRINGIDO;
        printConsoleHUD();
    }
    else if (key == GLFW_KEY_2 && interactionMode != MODE_RUNNING) {
        selectedAlgorithm = ALGO_MRESTRICTIVO;
        printConsoleHUD();
    }
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE) {
        if (interactionMode == MODE_READY || interactionMode == MODE_DONE) {
            interactionMode = MODE_RUNNING;
            globalGraph->resetColors();
            if (solver) delete solver;
            solver = new IterativeSolver(globalGraph, selectedAlgorithm);

            cout << "\nIniciando busqueda con Variable Mas "
                << (selectedAlgorithm == ALGO_MRESTRINGIDO ? "Restringida" : "Restrictiva") << "...\n";
            printConsoleHUD();
        }
    }
    else if (key == GLFW_KEY_C && interactionMode != MODE_RUNNING) {
        globalGraph->resetColors();
        interactionMode = MODE_READY;
        printConsoleHUD();
    }
    else if (key == GLFW_KEY_R && interactionMode != MODE_RUNNING) {
        delete globalGraph;
        globalGraph = new Graph(GraphGenerator::createRandomGraph(userNodes, userNeighbors));
        interactionMode = MODE_READY;
        printConsoleHUD();
    }
}

int main() {
    srand(static_cast<unsigned int>(time(NULL)));

    cout << "Graph Coloring" << endl;
    cout << "Number of nodes in the graph: ";
    cin >> userNodes;
    if (userNodes <= 0) userNodes = 1;

    cout << "Target number of neighbors per node (1 to 5): ";
    cin >> userNeighbors;
    if (userNeighbors < 1) userNeighbors = 1;
    if (userNeighbors > 5) userNeighbors = 5;

    globalGraph = new Graph(GraphGenerator::createRandomGraph(userNodes, userNeighbors));

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* window = glfwCreateWindow(static_cast<int>(MAX_WINDOW_PX), static_cast<int>(MAX_WINDOW_PX), "Graph Coloring", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    Renderer::init();
    glfwSetKeyCallback(window, key_callback);
    printConsoleHUD();

    int stepsPerFrame = 2500;

    while (!glfwWindowShouldClose(window)) {
        if (interactionMode == MODE_RUNNING && solver != nullptr) {
            for (int i = 0; i < stepsPerFrame && !solver->done; i++) {
                solver->step();
            }

            if (solver->done) {
                interactionMode = MODE_DONE;
                if (solver->success) {
                    cout << "\nExito: Grafo 3-coloreable.\n";
                }
                else if (solver->totalSteps > solver->MAX_STEPS) {
                    cout << "\nCancelado: Limite de " << solver->MAX_STEPS << " pasos excedido.\n";
                }
                else {
                    cout << "\nSin solucion: El grafo NO es 3-coloreable.\n";
                }
                cout << "Backtracking total: " << solver->backtrackCount << " veces\n";
                printConsoleHUD();
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);
        Renderer::drawGraph(*globalGraph);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (solver) delete solver;
    delete globalGraph;
    glfwTerminate();
    return 0;
}