#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <list>
#include <queue>
#include <stack>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>
#include <algorithm>

using namespace std;

// Estados posibles de un nodo
const int STATE_NORMAL = 0;
const int STATE_OBSTACLE = 1;
const int STATE_VISITED = 2;
const int STATE_PATH = 3;
const int STATE_START = 4;
const int STATE_END = 5;

// Modos de interacción de la interfaz
const int MODE_SELECT_START = 0;
const int MODE_SELECT_END = 1;
const int MODE_READY = 2;
const int MODE_RUNNING = 3;

const int ALGO_BFS = 1;
const int ALGO_DFS = 2;
const int ALGO_HILL = 3;
const int ALGO_ASTAR = 4;

float CELL_SIZE = 30.0f;
const float HUD_HEIGHT = 40.0f;
const int MESH_SIZE = 100;
const float MAX_WINDOW_PX = 900.0f;
const float MIN_CELL_SIZE = 4.0f;
const float MAX_CELL_SIZE = 30.0f;

const float NODE_RADIUS_RATIO = 0.22f;
const int   CIRCLE_SEGMENTS = 10;
const float EDGE_LINE_WIDTH = 1.0f;

struct Point {
    int row, col;
    Point() { row = 0; col = 0; }
    Point(int r, int c) { row = r; col = c; }
};

class Edge {
public:
    int to;
    float weight;
    Edge(int t, float w) { to = t; weight = w; }
};

class Node {
public:
    int index;
    Point position;
    int state;
    list<Edge> edges; // Lista de adyacencia
    Node(int i, Point pos) {
        index = i; position = pos; state = STATE_NORMAL;
    }
    void addEdge(int to, float weight) { edges.push_back(Edge(to, weight)); }
};

class Graph {
private:
    int numVertices;
    vector<Node> nodes;
public:
    Graph(int vertices) { numVertices = vertices; nodes.reserve(vertices); }
    void addNode(int index, Point position) { nodes.push_back(Node(index, position)); }
    void addEdge(int u, int v, float weight) {
        nodes[u].addEdge(v, weight); nodes[v].addEdge(u, weight);
    }
    Node& getNode(int index) { return nodes[index]; }
    int getNumVertices() const { return numVertices; }

    // Desconectar nodo 
    void disconnectNode(int index) {
        Node& node = nodes[index];
        list<Edge>::iterator it;
        for (it = node.edges.begin(); it != node.edges.end(); ++it) {
            int neighborIndex = it->to;
            Node& neighborNode = nodes[neighborIndex];
            list<Edge>::iterator edgeIt = neighborNode.edges.begin();
            // Eliminar arista recíproca en el vecino
            while (edgeIt != neighborNode.edges.end()) {
                if (edgeIt->to == index) { edgeIt = neighborNode.edges.erase(edgeIt); }
                else { ++edgeIt; }
            }
        }
        node.edges.clear();
    }
};

class Mesh {
private:
    int N;
    Graph graph;
    void buildConnections() {
        int deltaRow[8] = { -1, -1, -1,  0, 0,  1, 1, 1 };
        int deltaCol[8] = { -1,  0,  1, -1, 1, -1, 0, 1 };
        // Conectar cada celda con sus 8 vecinos
        for (int row = 0; row < N; row++) {
            for (int col = 0; col < N; col++) {
                int currentIndex = getIndex(row, col);
                for (int d = 0; d < 8; d++) {
                    int neighborRow = row + deltaRow[d];
                    int neighborCol = col + deltaCol[d];
                    if (neighborRow < 0 || neighborRow >= N || neighborCol < 0 || neighborCol >= N) continue;
                    int neighborIndex = getIndex(neighborRow, neighborCol);
                    if (neighborIndex <= currentIndex) continue; // Evitar duplicados

                    // Asignar peso mayor a las diagonales
                    bool isDiagonal = (deltaRow[d] != 0) && (deltaCol[d] != 0);
                    float weight = isDiagonal ? 1.41421356f : 1.0f;
                    graph.addEdge(currentIndex, neighborIndex, weight);
                }
            }
        }
    }
public:
    Mesh(int size) : N(size), graph(size* size) {
        for (int row = 0; row < N; row++) {
            for (int col = 0; col < N; col++) {
                graph.addNode(getIndex(row, col), Point(row, col));
            }
        }
        buildConnections();
    }
    int getSize() const { return N; }
    int getIndex(int row, int col) const { return row * N + col; }
    Graph& getGraph() { return graph; }

    vector<int> pickRandomIndices(int count) {
        int total = N * N;
        vector<int> indices(total);
        for (int i = 0; i < total; i++) indices[i] = i;
        for (int i = total - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            swap(indices[i], indices[j]);
        }
        indices.resize(count);
        return indices;
    }

    // Convertir nodos en obstaculos aislandolos del grafo
    void removeNodesPhysically(int percent) {
        int countToRemove = (N * N * percent) / 100;
        vector<int> indices = pickRandomIndices(countToRemove);
        for (size_t i = 0; i < indices.size(); i++) {
            int idx = indices[i];
            graph.getNode(idx).state = STATE_OBSTACLE;
            graph.disconnectNode(idx);
        }
    }
    // Convertir nodos en obstaculos visuales/lógicos
    void removeNodes(int percent) {
        int countToRemove = (N * N * percent) / 100;
        vector<int> indices = pickRandomIndices(countToRemove);
        for (size_t i = 0; i < indices.size(); i++) graph.getNode(indices[i]).state = STATE_OBSTACLE;
    }
    void resetMesh() {
        for (int i = 0; i < graph.getNumVertices(); i++) graph.getNode(i).state = STATE_NORMAL;
    }
    // Limpiar variables de busqueda para ejecutar otro algoritmo
    void clearSearchStates() {
        for (int i = 0; i < graph.getNumVertices(); i++) {
            Node& node = graph.getNode(i);
            if (node.state == STATE_VISITED || node.state == STATE_PATH) node.state = STATE_NORMAL;
        }
    }
};

struct AStarEntry {
    int index; float f;
    bool operator<(const AStarEntry& other) const { return f > other.f; }
};

class Algorithms {
private:
    // Reconstruir la ruta navegando los padres hacia atrás
    static void reconstructPath(vector<int>& parent, int start, int target, vector<int>& outPath) {
        outPath.clear();
        int current = target;
        while (current != -1) {
            outPath.push_back(current);
            if (current == start) break;
            current = parent[current];
        }
        reverse(outPath.begin(), outPath.end());
    }
public:
    static float euclideanHeuristic(Point a, Point b) {
        float dx = static_cast<float>(a.col - b.col);
        float dy = static_cast<float>(a.row - b.row);
        return sqrt(dx * dx + dy * dy);
    }
    static float calculatePathDistance(Mesh& mesh, vector<int>& path, bool useEuclidean) {
        if (path.size() < 2) return 0.0f;
        if (!useEuclidean) return static_cast<float>(path.size() - 1);
        float total = 0.0f;
        for (size_t i = 0; i + 1 < path.size(); i++) {
            Point a = mesh.getGraph().getNode(path[i]).position;
            Point b = mesh.getGraph().getNode(path[i + 1]).position;
            total += euclideanHeuristic(a, b);
        }
        return total;
    }

    static void BFS(Mesh& mesh, int start, int target, vector<int>& outPath, vector<int>& outExplored) {
        outPath.clear(); outExplored.clear();
        int total = mesh.getGraph().getNumVertices();
        vector<bool> visited(total, false);
        vector<int> parent(total, -1);

        // Estructura Cola (FIFO) para expansión en anillos
        queue<int> cola;
        cola.push(start); visited[start] = true;
        bool found = false;

        while (!cola.empty()) {
            int current = cola.front(); cola.pop();
            outExplored.push_back(current);
            if (current == target) { found = true; break; }

            // Evaluar vecinos conectados
            Node& currentNode = mesh.getGraph().getNode(current);
            for (auto it = currentNode.edges.begin(); it != currentNode.edges.end(); ++it) {
                int neighbor = it->to;
                if (!visited[neighbor] && mesh.getGraph().getNode(neighbor).state != STATE_OBSTACLE) {
                    visited[neighbor] = true;
                    parent[neighbor] = current;
                    cola.push(neighbor);
                }
            }
        }
        if (found) reconstructPath(parent, start, target, outPath);
    }

    static void DFS(Mesh& mesh, int start, int target, vector<int>& outPath, vector<int>& outExplored) {
        outPath.clear(); outExplored.clear();
        int total = mesh.getGraph().getNumVertices();
        vector<bool> visited(total, false);
        vector<int> parent(total, -1);

        // Estructura Pila (LIFO) para profundidad extrema
        stack<int> pila;
        pila.push(start); visited[start] = true;
        bool found = false;

        while (!pila.empty()) {
            int current = pila.top(); pila.pop();
            outExplored.push_back(current);
            if (current == target) { found = true; break; }

            Node& currentNode = mesh.getGraph().getNode(current);
            for (auto it = currentNode.edges.begin(); it != currentNode.edges.end(); ++it) {
                int neighbor = it->to;
                if (!visited[neighbor] && mesh.getGraph().getNode(neighbor).state != STATE_OBSTACLE) {
                    visited[neighbor] = true;
                    parent[neighbor] = current;
                    pila.push(neighbor);
                }
            }
        }
        if (found) reconstructPath(parent, start, target, outPath);
    }

    static void HillClimbing(Mesh& mesh, int start, int target, vector<int>& outPath, vector<int>& outExplored) {
        outPath.clear(); outExplored.clear();
        int total = mesh.getGraph().getNumVertices();
        vector<bool> visited(total, false);
        Point targetPos = mesh.getGraph().getNode(target).position;
        int current = start; visited[start] = true;
        outExplored.push_back(start); outPath.push_back(start);
        bool found = (current == target);

        while (!found) {
            Point currentPos = mesh.getGraph().getNode(current).position;
            float currentHeuristic = euclideanHeuristic(currentPos, targetPos);
            int bestNeighbor = -1; float bestHeuristic = currentHeuristic;

            // Buscar agresivamente el vecino que minimice la distancia
            Node& currentNode = mesh.getGraph().getNode(current);
            for (auto it = currentNode.edges.begin(); it != currentNode.edges.end(); ++it) {
                int neighbor = it->to;
                Node& neighborNode = mesh.getGraph().getNode(neighbor);
                if (visited[neighbor] || neighborNode.state == STATE_OBSTACLE) continue;

                float neighborHeuristic = euclideanHeuristic(neighborNode.position, targetPos);
                if (neighborHeuristic < bestHeuristic) {
                    bestHeuristic = neighborHeuristic;
                    bestNeighbor = neighbor;
                }
            }
            if (bestNeighbor == -1) break; // Atascado en mínimo local

            visited[bestNeighbor] = true;
            outExplored.push_back(bestNeighbor);
            outPath.push_back(bestNeighbor);
            current = bestNeighbor;
            if (current == target) found = true;
        }
        if (!found) outPath.clear();
    }

    static void AStar(Mesh& mesh, int start, int target, vector<int>& outPath, vector<int>& outExplored) {
        outPath.clear(); outExplored.clear();
        int total = mesh.getGraph().getNumVertices();
        vector<float> gScore(total, -1.0f);
        vector<int> parent(total, -1);
        vector<bool> closed(total, false);
        Point targetPos = mesh.getGraph().getNode(target).position;
        Point startPos = mesh.getGraph().getNode(start).position;

        // Cola de prioridad para procesar primero los de menor costo f = g + h
        priority_queue<AStarEntry> openSet;
        gScore[start] = 0.0f;
        openSet.push({ start, euclideanHeuristic(startPos, targetPos) });
        bool found = false;

        while (!openSet.empty()) {
            int current = openSet.top().index;
            openSet.pop();

            if (closed[current]) continue;
            closed[current] = true;
            outExplored.push_back(current);

            if (current == target) { found = true; break; }

            // Calcular costos acumulados (gScore) de vecinos
            Node& currentNode = mesh.getGraph().getNode(current);
            for (auto it = currentNode.edges.begin(); it != currentNode.edges.end(); ++it) {
                int neighbor = it->to;
                if (closed[neighbor] || mesh.getGraph().getNode(neighbor).state == STATE_OBSTACLE) continue;

                float tentativeG = gScore[current] + it->weight;
                if (gScore[neighbor] < 0.0f || tentativeG < gScore[neighbor]) {
                    gScore[neighbor] = tentativeG;
                    parent[neighbor] = current;
                    openSet.push({ neighbor, tentativeG + euclideanHeuristic(mesh.getGraph().getNode(neighbor).position, targetPos) });
                }
            }
        }
        if (found) reconstructPath(parent, start, target, outPath);
    }
};

Mesh myMesh(MESH_SIZE);
vector<int> currentPath;
vector<int> currentExplored;

int startNode = -1;
int targetNode = -1;

int interactionMode = MODE_SELECT_START;
int selectedAlgorithm = ALGO_BFS;
float lastDistance = 0.0f;
int animationCursor = 0;

int currentHoverRow = -1;
int currentHoverCol = -1;

void printConsoleHUD() {
    cout << "\r                                                                                                     \r";

    cout << "[MODO] ";
    if (interactionMode == MODE_SELECT_START) cout << "Click para Inicio | ";
    else if (interactionMode == MODE_SELECT_END) cout << "Click para Destino | ";
    else if (interactionMode == MODE_READY) cout << "ENTER: ejecutar, C: limpiar, R: reset | ";
    else if (interactionMode == MODE_RUNNING) cout << "Ejecutando... | ";

    cout << "[ALGO] ";
    if (selectedAlgorithm == ALGO_BFS) cout << "BFS";
    else if (selectedAlgorithm == ALGO_DFS) cout << "DFS";
    else if (selectedAlgorithm == ALGO_HILL) cout << "Hill Climbing";
    else if (selectedAlgorithm == ALGO_ASTAR) cout << "A*";

    cout << " | [INICIO] ";
    if (startNode != -1) {
        Point p = myMesh.getGraph().getNode(startNode).position;
        cout << "(" << p.row << ", " << p.col << ")";
    }
    else {
        cout << "--";
    }

    cout << " | [DESTINO] ";
    if (targetNode != -1) {
        Point p = myMesh.getGraph().getNode(targetNode).position;
        cout << "(" << p.row << ", " << p.col << ")";
    }
    else {
        cout << "--";
    }

    cout << flush;
}


class Renderer {
private:
    static void cellCenter(int row, int col, float& outX, float& outY) {
        outX = col * CELL_SIZE + CELL_SIZE * 0.5f;
        outY = row * CELL_SIZE + CELL_SIZE * 0.5f;
    }
    static void drawCircle(float cx, float cy, float radius, float r, float g, float b) {
        glColor3f(r, g, b);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
            float angle = (2.0f * 3.14159265f * i) / CIRCLE_SEGMENTS;
            glVertex2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
        }
        glEnd();
    }
    static bool isExplored(vector<int>& explored, int cursor, int index) {
        int limit = min(cursor, static_cast<int>(explored.size()));
        for (int i = 0; i < limit; i++) {
            if (explored[i] == index) return true;
        }
        return false;
    }
    static bool isInPath(vector<int>& path, int index) {
        for (size_t i = 0; i < path.size(); i++) {
            if (path[i] == index) return true;
        }
        return false;
    }

public:
    static void init(int gridSizeCells) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float sizePx = gridSizeCells * CELL_SIZE;
        glOrtho(0.0, sizePx, sizePx + HUD_HEIGHT, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(EDGE_LINE_WIDTH);
    }
    static void drawEdges(Mesh& mesh) {
        glColor3f(0.0f, 0.0f, 0.0f);
        glBegin(GL_LINES);
        int total = mesh.getGraph().getNumVertices();
        for (int i = 0; i < total; i++) {
            Node& node = mesh.getGraph().getNode(i);
            if (node.state == STATE_OBSTACLE) continue;
            float x1, y1; cellCenter(node.position.row, node.position.col, x1, y1);
            for (auto it = node.edges.begin(); it != node.edges.end(); ++it) {
                int neighborIndex = it->to;
                if (neighborIndex <= i) continue;
                Node& neighborNode = mesh.getGraph().getNode(neighborIndex);
                if (neighborNode.state == STATE_OBSTACLE) continue;
                float x2, y2; cellCenter(neighborNode.position.row, neighborNode.position.col, x2, y2);
                glVertex2f(x1, y1); glVertex2f(x2, y2);
            }
        }
        glEnd();
    }
    static void drawNodes(Mesh& mesh, vector<int>& explored, int animationCursor,
        vector<int>& path, int startIndex, int targetIndex, int currentInteractionMode) {
        int total = mesh.getGraph().getNumVertices();

        // Iterar y dibujar colores por estado
        for (int i = 0; i < total; i++) {
            Node& node = mesh.getGraph().getNode(i);
            float r = 0.0f, g = 0.0f, b = 0.0f;
            if (node.state == STATE_OBSTACLE) { r = 0.8f; g = 0.8f; b = 0.8f; } // Nodos eliminados
            if (isExplored(explored, animationCursor, i)) { r = 0.0f; g = 1.0f; b = 0.0f; } // Explorado
            if (currentInteractionMode != MODE_RUNNING && isInPath(path, i)) { r = 0.1f; g = 0.3f; b = 1.0f; } // Path
            if (i == startIndex) { r = 1.0f; g = 0.8f; b = 0.0f; } // Start
            if (i == targetIndex) { r = 0.8f; g = 0.0f; b = 0.8f; } // Target

            float cx, cy; cellCenter(node.position.row, node.position.col, cx, cy);
            drawCircle(cx, cy, CELL_SIZE * NODE_RADIUS_RATIO, r, g, b);
        }
    }
};


void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    float gridHeightPx = myMesh.getSize() * CELL_SIZE;
    if (ypos < 0 || ypos >= gridHeightPx || xpos < 0 || xpos >= gridHeightPx) {
        glfwSetWindowTitle(window, "Mesh Search Visualizer");
        return;
    }

    int col = static_cast<int>(xpos / CELL_SIZE);
    int row = static_cast<int>(ypos / CELL_SIZE);

    if (row >= 0 && row < myMesh.getSize() && col >= 0 && col < myMesh.getSize()) {
        string title = "Mesh Search Visualizer - Hover: Fila " + to_string(row) + ", Col " + to_string(col);
        glfwSetWindowTitle(window, title.c_str());
    }
    else {
        glfwSetWindowTitle(window, "Mesh Search Visualizer");
    }
}

// Interacción para colocar inicio y objetivo
void mouse_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action != GLFW_PRESS || button != GLFW_MOUSE_BUTTON_LEFT) return;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    float gridHeightPx = myMesh.getSize() * CELL_SIZE;
    if (ypos < 0 || ypos >= gridHeightPx) return;

    int col = static_cast<int>(xpos / CELL_SIZE);
    int row = static_cast<int>(ypos / CELL_SIZE);

    if (row < 0 || row >= myMesh.getSize() || col < 0 || col >= myMesh.getSize()) return;

    int clickedIndex = myMesh.getIndex(row, col);
    Node& clickedNode = myMesh.getGraph().getNode(clickedIndex);
    if (clickedNode.state == STATE_OBSTACLE) return;

    if (interactionMode == MODE_SELECT_START) {
        startNode = clickedIndex;
        clickedNode.state = STATE_START;
        interactionMode = MODE_SELECT_END;
    }
    else if (interactionMode == MODE_SELECT_END) {
        targetNode = clickedIndex;
        clickedNode.state = STATE_END;
        interactionMode = MODE_READY;
    }
    printConsoleHUD();
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_1) selectedAlgorithm = ALGO_BFS;
    else if (key == GLFW_KEY_2) selectedAlgorithm = ALGO_DFS;
    else if (key == GLFW_KEY_3) selectedAlgorithm = ALGO_HILL;
    else if (key == GLFW_KEY_4) selectedAlgorithm = ALGO_ASTAR;
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE) {
        if (interactionMode == MODE_READY) {
            string nombreAlgo = "";

            if (selectedAlgorithm == ALGO_BFS) {
                Algorithms::BFS(myMesh, startNode, targetNode, currentPath, currentExplored);
                nombreAlgo = "BFS";
            }
            else if (selectedAlgorithm == ALGO_DFS) {
                Algorithms::DFS(myMesh, startNode, targetNode, currentPath, currentExplored);
                nombreAlgo = "DFS";
            }
            else if (selectedAlgorithm == ALGO_HILL) {
                Algorithms::HillClimbing(myMesh, startNode, targetNode, currentPath, currentExplored);
                nombreAlgo = "Hill Climbing";
            }
            else if (selectedAlgorithm == ALGO_ASTAR) {
                Algorithms::AStar(myMesh, startNode, targetNode, currentPath, currentExplored);
                nombreAlgo = "A*";
            }

            lastDistance = Algorithms::calculatePathDistance(myMesh, currentPath, true);
            cout << "\n[REGISTRO] Algoritmo: " << nombreAlgo << " | Distancia: " << lastDistance << "\n";

            animationCursor = 0;
            interactionMode = MODE_RUNNING;
        }
    }
    else if (key == GLFW_KEY_C) {
        myMesh.clearSearchStates();
        currentPath.clear();
        currentExplored.clear();
        animationCursor = 0;
        interactionMode = MODE_READY;
    }
    else if (key == GLFW_KEY_R) {
        myMesh.resetMesh();
        myMesh.removeNodes(20);
        currentPath.clear();
        currentExplored.clear();
        startNode = -1;
        targetNode = -1;
        animationCursor = 0;
        interactionMode = MODE_SELECT_START;
    }
    printConsoleHUD();
}

int main() {
    srand(static_cast<unsigned int>(time(NULL)));

    CELL_SIZE = MAX_WINDOW_PX / static_cast<float>(MESH_SIZE);
    if (CELL_SIZE < MIN_CELL_SIZE) CELL_SIZE = MIN_CELL_SIZE;
    if (CELL_SIZE > MAX_CELL_SIZE) CELL_SIZE = MAX_CELL_SIZE;

    myMesh.removeNodes(20);

    if (!glfwInit()) {
        cout << "Failed to initialize GLFW" << endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    int gridPx = static_cast<int>(myMesh.getSize() * CELL_SIZE);
    int windowHeightPx = gridPx + static_cast<int>(HUD_HEIGHT);

    GLFWwindow* window = glfwCreateWindow(gridPx, windowHeightPx, "Mesh Search Visualizer", NULL, NULL);
    if (!window) {
        cout << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    Renderer::init(myMesh.getSize());

    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_callback);

    printConsoleHUD();
    cout << "\n\nPresione los numeros [1-4] en para cambiar el algoritmo.\n";

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();

        if (currentTime - lastTime >= 0.016) {
            if (interactionMode == MODE_RUNNING) {
                if (animationCursor < static_cast<int>(currentExplored.size())) {
                    animationCursor++;
                }
                else {
                    interactionMode = MODE_READY;
                    printConsoleHUD();
                }
            }
            lastTime = currentTime;
        }

        glClear(GL_COLOR_BUFFER_BIT);

        Renderer::drawEdges(myMesh);
        Renderer::drawNodes(myMesh, currentExplored, animationCursor, currentPath,
            startNode, targetNode, interactionMode);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}