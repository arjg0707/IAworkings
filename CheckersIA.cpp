#include <iostream>
#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

using namespace std;

const int EMPTY = 0;
const int BLACK = 1;   
const int RED = 2;     

const int BOARD_SIZE = 8;
const float WINDOW_WIDTH = 640.0f;
const float WINDOW_HEIGHT = 640.0f;
const float CELL_SIZE = WINDOW_WIDTH / static_cast<float>(BOARD_SIZE);

const int MAX_DEPTH_DEFAULT = 1;
const int INF_VALUE = 9999;

bool gClickReceived = false;
double gClickX = 0.0;
double gClickY = 0.0;

void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        glfwGetCursorPos(window, &gClickX, &gClickY);
        gClickReceived = true;
    }
}

int getOppositeColor(int color) {
    if (color == BLACK) return RED;
    return BLACK;
}

// Configurar las piezas en sus posiciones iniciales
void initStandardBoard(int board[BOARD_SIZE][BOARD_SIZE]) {
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            board[row][col] = EMPTY;
        }
    }
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if ((row + col) % 2 == 1) board[row][col] = BLACK;
        }
    }
    for (int row = 5; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if ((row + col) % 2 == 1) board[row][col] = RED;
        }
    }
}

// Estructura para almacenar una jugada
struct Move {
    int fromRow, fromCol;
    int toRow, toCol;
    bool isCapture;
    int capturedRow, capturedCol;

    Move() {
        fromRow = -1; fromCol = -1;
        toRow = -1; toCol = -1;
        isCapture = false;
        capturedRow = -1; capturedCol = -1;
    }
};

// Nodos como estados del tablero
class Node {
private:
    int board[BOARD_SIZE][BOARD_SIZE];
    int turn;
    int score;
    vector<Node*> children;
    Move originMove;

public:
    Node() {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                board[row][col] = EMPTY;
            }
        }
        turn = BLACK;
        score = 0;
    }

    Node(int sourceBoard[BOARD_SIZE][BOARD_SIZE], int currentTurn) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                board[row][col] = sourceBoard[row][col];
            }
        }
        turn = currentTurn;
        score = 0;
    }

    ~Node() {
        for (size_t i = 0; i < children.size(); i++) {
            delete children[i];
        }
        children.clear();
    }

    // Retornar movimientos simples y capturas en una sola lista
    vector<Move> getValidMoves(int color) {
        vector<Move> moves;
        int rowDir = (color == RED) ? -1 : 1;

        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board[row][col] != color) continue;

                int colDirs[2] = { -1, 1 };
                for (int i = 0; i < 2; i++) {
                    int colDelta = colDirs[i];
                    int newRow = row + rowDir;
                    int newCol = col + colDelta;

                    if (newRow < 0 || newRow >= BOARD_SIZE || newCol < 0 || newCol >= BOARD_SIZE) continue;

                    // Movimiento simple
                    if (board[newRow][newCol] == EMPTY) {
                        Move simpleMove;
                        simpleMove.fromRow = row; simpleMove.fromCol = col;
                        simpleMove.toRow = newRow; simpleMove.toCol = newCol;
                        simpleMove.isCapture = false;
                        moves.push_back(simpleMove);
                    }
                    // Movimiento de captura
                    else if (board[newRow][newCol] == getOppositeColor(color)) {
                        int jumpRow = row + (2 * rowDir);
                        int jumpCol = col + (2 * colDelta);

                        if (jumpRow >= 0 && jumpRow < BOARD_SIZE && jumpCol >= 0 && jumpCol < BOARD_SIZE) {
                            if (board[jumpRow][jumpCol] == EMPTY) {
                                Move captureMove;
                                captureMove.fromRow = row; captureMove.fromCol = col;
                                captureMove.toRow = jumpRow; captureMove.toCol = jumpCol;
                                captureMove.isCapture = true;
                                captureMove.capturedRow = newRow; captureMove.capturedCol = newCol;
                                moves.push_back(captureMove);
                            }
                        }
                    }
                }
            }
        }
        return moves;
    }

    // Aplicar un movimiento en otra matriz
    void applyMove(const Move& move, int resultBoard[BOARD_SIZE][BOARD_SIZE]) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                resultBoard[row][col] = board[row][col];
            }
        }

        resultBoard[move.toRow][move.toCol] = resultBoard[move.fromRow][move.fromCol];
        resultBoard[move.fromRow][move.fromCol] = EMPTY;

        if (move.isCapture) {
            resultBoard[move.capturedRow][move.capturedCol] = EMPTY;
        }
    }

    // Generar estados posibles
    void generateChildren() {
        if (!children.empty()) return;

        vector<Move> validMoves = getValidMoves(turn);

        for (size_t i = 0; i < validMoves.size(); i++) {
            int resultBoard[BOARD_SIZE][BOARD_SIZE];
            applyMove(validMoves[i], resultBoard);

            Node* child = new Node(resultBoard, getOppositeColor(turn));
            child->originMove = validMoves[i];
            children.push_back(child);
        }
    }

    // Heuristica(Negras-Rojas)
    int evaluateHeuristic() {
        int blackCount = 0, redCount = 0;

        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board[row][col] == BLACK) blackCount++;
                else if (board[row][col] == RED) redCount++;
            }
        }

        score = blackCount - redCount;
        return score;
    }

    // Verificar si un jugador se ya no tiene piezas o movimientos
    bool checkWinner(int& winnerColor) {
        int blackCount = 0, redCount = 0;

        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board[row][col] == BLACK) blackCount++;
                else if (board[row][col] == RED) redCount++;
            }
        }

        if (blackCount == 0) { winnerColor = RED; return true; }
        if (redCount == 0) { winnerColor = BLACK; return true; }

        vector<Move> turnMoves = getValidMoves(turn);
        if (turnMoves.empty()) {
            winnerColor = getOppositeColor(turn);
            return true;
        }

        return false;
    }

    int getScore() const { return score; }
    void setScore(int val) { score = val; }
    vector<Node*>& getChildren() { return children; }
    Move getOriginMove() const { return originMove; }
    int getTurn() const { return turn; }

    void getBoard(int dest[BOARD_SIZE][BOARD_SIZE]) const {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                dest[row][col] = board[row][col];
            }
        }
    }
};

// Para Minimax
class Tree {
private:
    Node* root;
    int maxDepth;

public:
    Tree(Node* initRoot, int depth) {
        root = initRoot;
        maxDepth = depth;
    }

    ~Tree() {
        delete root;
    }

    // Minimax
    int minimax(Node* node, int currentDepth, bool isMaximizer) {
        int winner;
        if (node->checkWinner(winner) || currentDepth == 0) {
            return node->evaluateHeuristic();
        }

        node->generateChildren();
        vector<Node*>& children = node->getChildren();

        if (children.empty()) return node->evaluateHeuristic();

        if (isMaximizer) {
            int bestVal = -INF_VALUE;
            for (size_t i = 0; i < children.size(); i++) {
                int val = minimax(children[i], currentDepth - 1, false);
                if (val > bestVal) bestVal = val;
            }
            return bestVal;
        }
        else {
            int worstVal = INF_VALUE;
            for (size_t i = 0; i < children.size(); i++) {
                int val = minimax(children[i], currentDepth - 1, true);
                if (val < worstVal) worstVal = val;
            }
            return worstVal;
        }
    }

    // Evaluar nodos hijos y retornar la mejor jugada 
    Move getBestMove() {
        root->generateChildren();
        vector<Node*>& children = root->getChildren();

        Move bestMove;
        if (children.empty()) return bestMove;

        bool isMaximizer = (root->getTurn() == BLACK);
        int bestVal = isMaximizer ? -INF_VALUE : INF_VALUE;

        for (size_t i = 0; i < children.size(); i++) {
            int val = minimax(children[i], maxDepth - 1, !isMaximizer);

            bool isBetter = (isMaximizer && val > bestVal) || (!isMaximizer && val < bestVal);
            if (isBetter) {
                bestVal = val;
                bestMove = children[i]->getOriginMove();
            }
        }
        return bestMove;
    }
};

class Renderer {
private:
    GLFWwindow* window;
    float windowWidth;
    float windowHeight;

    void drawCircle(float cx, float cy, float radius, int segments) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= segments; i++) {
            float angle = (i / static_cast<float>(segments)) * 2.0f * 3.14159265f;
            float x = cx + (radius * cos(angle));
            float y = cy + (radius * sin(angle));
            glVertex2f(x, y);
        }
        glEnd();
    }

public:
    Renderer() {
        window = NULL;
        windowWidth = 0.0f;
        windowHeight = 0.0f;
    }

    ~Renderer() {
        if (window != NULL) glfwDestroyWindow(window);
        glfwTerminate();
    }

    bool init(float width, float height, const char* title) {
        if (!glfwInit()) {
            cout << "Failed to initialize GLFW" << endl;
            return false;
        }

        window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title, NULL, NULL);
        if (window == NULL) {
            cout << "Failed to create GLFW window" << endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            cout << "Failed to initialize GLAD" << endl;
            glfwDestroyWindow(window);
            window = NULL;
            glfwTerminate();
            return false;
        }

        glfwSetMouseButtonCallback(window, mouseCallback);

        windowWidth = width;
        windowHeight = height;

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, static_cast<double>(windowWidth), static_cast<double>(windowHeight), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);

        return true;
    }

    void drawBoard() {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if ((row + col) % 2 == 0) glColor3f(0.90f, 0.90f, 0.82f);
                else glColor3f(0.30f, 0.20f, 0.15f);

                float x0 = col * CELL_SIZE;
                float y0 = row * CELL_SIZE;
                float x1 = x0 + CELL_SIZE;
                float y1 = y0 + CELL_SIZE;

                glBegin(GL_QUADS);
                glVertex2f(x0, y0);
                glVertex2f(x1, y0);
                glVertex2f(x1, y1);
                glVertex2f(x0, y1);
                glEnd();
            }
        }
    }

    void drawPieces(int board[BOARD_SIZE][BOARD_SIZE]) {
        float radius = CELL_SIZE * 0.38f;

        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board[row][col] == EMPTY) continue;

                float cx = (col * CELL_SIZE) + (CELL_SIZE * 0.5f);
                float cy = (row * CELL_SIZE) + (CELL_SIZE * 0.5f);

                if (board[row][col] == BLACK) glColor3f(0.05f, 0.05f, 0.05f);
                else glColor3f(0.75f, 0.10f, 0.10f);

                drawCircle(cx, cy, radius, 32);
            }
        }
    }

    void drawSelection(int row, int col) {
        if (row < 0 || col < 0) return;

        float x0 = col * CELL_SIZE;
        float y0 = row * CELL_SIZE;
        float x1 = x0 + CELL_SIZE;
        float y1 = y0 + CELL_SIZE;

        glColor3f(1.0f, 1.0f, 0.0f);
        glLineWidth(3.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x0, y0);
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, y1);
        glEnd();
    }

    void update(int board[BOARD_SIZE][BOARD_SIZE], int selectedRow, int selectedCol) {
        glClear(GL_COLOR_BUFFER_BIT);
        drawBoard();
        drawPieces(board);
        drawSelection(selectedRow, selectedCol);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    void screenToGrid(double px, double py, int& outRow, int& outCol) {
        outCol = static_cast<int>(px / CELL_SIZE);
        outRow = static_cast<int>(py / CELL_SIZE);
    }

    GLFWwindow* getWindow() const { return window; }

    bool shouldClose() const {
        if (window == NULL) return true;
        return glfwWindowShouldClose(window) != 0;
    }
};


class Game {
private:
    Node* currentState;
    Renderer renderer;
    int aiDepth;
    bool isGameOver;
    bool pieceSelected;
    int selectedRow;
    int selectedCol;

    void printMove(int color, const Move& m) {
        string player = (color == RED) ? "Jugador (ROJO)" : "IA (NEGRO)";
        cout << player << " mueve de (" << m.fromCol << ", " << m.fromRow
            << ") a (" << m.toCol << ", " << m.toRow << ")";
        if (m.isCapture) {
            cout << " - Captura realizada!";
        }
        cout << endl;
    }

    bool findMove(const vector<Move>& moves, int fRow, int fCol, int tRow, int tCol, Move& foundMove) {
        for (size_t i = 0; i < moves.size(); i++) {
            if (moves[i].fromRow == fRow && moves[i].fromCol == fCol &&
                moves[i].toRow == tRow && moves[i].toCol == tCol) {
                foundMove = moves[i];
                return true;
            }
        }
        return false;
    }

public:
    Game() {
        currentState = NULL;
        aiDepth = MAX_DEPTH_DEFAULT;
        isGameOver = false;
        pieceSelected = false;
        selectedRow = -1;
        selectedCol = -1;
    }

    ~Game() {
        if (currentState != NULL) delete currentState;
    }

    void initGame() {
        cout << "--- DAMAS CON IA (MINIMAX) ---" << endl;
        cout << "Ingrese la profundidad de busqueda para la IA: ";
        cin >> aiDepth;
        if (aiDepth < 1) aiDepth = MAX_DEPTH_DEFAULT;

        int initTurnChoice = 1;
        cout << "\n¿Quien empieza primero?" << endl;
        cout << "1. Jugador (ROJO)" << endl;
        cout << "2. IA (NEGRO)" << endl;
        cout << "Elige (1 o 2): ";
        cin >> initTurnChoice;

        int startingTurn = (initTurnChoice == 2) ? BLACK : RED;

        int initBoard[BOARD_SIZE][BOARD_SIZE];
        initStandardBoard(initBoard);

        currentState = new Node(initBoard, startingTurn);
        isGameOver = false;

        bool initialized = renderer.init(WINDOW_WIDTH, WINDOW_HEIGHT, "Checkers vs AI - Minimax");
        if (!initialized) isGameOver = true;
    }

    void playerTurn() {
        pieceSelected = false;
        selectedRow = -1;
        selectedCol = -1;
        bool moveMade = false;

        while (!moveMade && !renderer.shouldClose()) {
            int currentBoard[BOARD_SIZE][BOARD_SIZE];
            currentState->getBoard(currentBoard);
            renderer.update(currentBoard, selectedRow, selectedCol);

            if (!gClickReceived) continue;

            gClickReceived = false;
            int clickRow, clickCol;
            renderer.screenToGrid(gClickX, gClickY, clickRow, clickCol);

            if (clickRow < 0 || clickRow >= BOARD_SIZE || clickCol < 0 || clickCol >= BOARD_SIZE) continue;

            if (!pieceSelected) {
                if (currentBoard[clickRow][clickCol] == RED) {
                    pieceSelected = true;
                    selectedRow = clickRow;
                    selectedCol = clickCol;
                }
                continue;
            }
            vector<Move> validMoves = currentState->getValidMoves(RED);
            Move chosenMove;
            bool found = findMove(validMoves, selectedRow, selectedCol, clickRow, clickCol, chosenMove);

            if (found) {
                int resultBoard[BOARD_SIZE][BOARD_SIZE];
                currentState->applyMove(chosenMove, resultBoard);

                printMove(RED, chosenMove); 

                delete currentState;
                currentState = new Node(resultBoard, BLACK); 
                moveMade = true;
            }
            else {
                // Actualizar seleccion
                if (currentBoard[clickRow][clickCol] == RED) {
                    selectedRow = clickRow;
                    selectedCol = clickCol;
                }
                else {
                    pieceSelected = false;
                    selectedRow = -1;
                    selectedCol = -1;
                }
            }
        }

        if (renderer.shouldClose()) isGameOver = true;
    }

    // Construir un arbol temporal, ejecuta Minimax y aplica el resultado
    void aiTurn() {
     
        int currentBoard[BOARD_SIZE][BOARD_SIZE];
        currentState->getBoard(currentBoard);
        renderer.update(currentBoard, -1, -1);

        cout << "\nTurno de la IA (BLACK), calculando jugada..." << endl;

        Node* searchRoot = new Node(currentBoard, BLACK);
        Tree searchTree(searchRoot, aiDepth);

        Move bestMove = searchTree.getBestMove();

        if (bestMove.fromRow == -1) return;

        printMove(BLACK, bestMove); 

        int resultBoard[BOARD_SIZE][BOARD_SIZE];
        currentState->applyMove(bestMove, resultBoard);
        delete currentState;
        currentState = new Node(resultBoard, RED); // Cambiar de turno
    }

    bool checkEndGame() {
        int winner;
        if (currentState->checkWinner(winner)) {
            isGameOver = true;
            if (winner == BLACK) cout << "\n*** La IA (BLACK) ha ganado la partida. ***" << endl;
            else cout << "\n*** El jugador (RED) ha ganado la partida. ***" << endl;
            return true;
        }
        return false;
    }

    void run() {
        cout << "\n¡A jugar!" << endl;

        while (!isGameOver && !renderer.shouldClose()) {

            // Llamr al turno correspondiente según el estado actual
            if (currentState->getTurn() == RED) {
                playerTurn();
            }
            else {
                aiTurn();
            }

            if (checkEndGame()) break;
        }

        if (isGameOver) {
            cout << "Game Over!" << endl;
            while (!renderer.shouldClose()) {
                int finalBoard[BOARD_SIZE][BOARD_SIZE];
                currentState->getBoard(finalBoard);
                renderer.update(finalBoard, -1, -1);
            }
        }
    }
};

int main() {
    Game checkers;
    checkers.initGame();
    checkers.run();
    return 0;
}