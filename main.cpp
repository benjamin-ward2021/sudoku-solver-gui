#include"imgui.h"
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"

#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include <vector>
using std::vector;
#include <math.h>
using std::max;
#include "stb_image.h"
#include <string>
using std::to_string;

// Vertex Shader source code
const char *vertexShaderSource =
"#version 330 core\n"
"layout(location = 0) in vec2 aPos;\n"
"layout(location = 1) in vec2 aTexCoord;\n"

"out vec2 TexCoord;\n"

"uniform float size;\n"

"void main()\n"
"{\n"
"	gl_Position = vec4(aPos * size, 0.0, 1.0);\n"
"	TexCoord = vec2(aTexCoord);\n"
"}\0";
// Fragment Shader source code
const char *fragmentShaderSource =
"#version 330 core\n"
"out vec4 FragColor;\n"

"in vec2 TexCoord;\n"

"// texture sampler\n"
"uniform sampler2D texture1;\n"

"void main()\n"
"{\n"
"	FragColor = texture(texture1, TexCoord);\n"
"}\0";

unsigned int initShaders();
void initTexture(unsigned int &texture);
ImVec2 vertexToPixel(float vertexX, float vertexY, int width, int height);
vector<ImVec2> generateCellCoordinates(float *positionVertices, unsigned int width, unsigned int height, float offset);
void solveSudoku(vector<vector<char>> &board);

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// ImGui will look weird on monitors that aren't scaled at 100% unless we scale it
	// (4k displays for example are usually scaled at 250%)
	float glfwScaleX, glfwScaleY;
	glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &glfwScaleX, &glfwScaleY);
	float dpiScale = (glfwScaleX + glfwScaleY) / 2;
	int WIDTH = 800 * dpiScale;
	int HEIGHT = 800 * dpiScale;

	GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Sudoku solver", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	gladLoadGL();
	glViewport(0, 0, WIDTH, HEIGHT);

	unsigned int shaderProgram = initShaders();

	unsigned int sudokuTexture;
	initTexture(sudokuTexture);

	float positionVertices[] = {
		 0.0f,  0.5f, // top right
		 0.0f, -0.5f, // bottom right
		-1.0f, -0.5f, // bottom left
		-1.0f,  0.5f  // top left 
	};
	float textureVertices[] = {
		1.0f, 1.0f, // top right
		1.0f, 0.0f, // bottom right
		0.0f, 0.0f, // bottom left
		0.0f, 1.0f  // top left 
	};
	unsigned int indices[] = {
		0, 1, 3, // first triangle
		1, 2, 3  // second triangle
	};

	unsigned int VAO, VBO[2], EBO;

	glGenVertexArrays(1, &VAO);
	glGenBuffers(2, VBO);	
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(positionVertices), positionVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(textureVertices), textureVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// position attribute
	glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	// texture coord attribute
	glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);

	// This is unnecessary since we only have one VAO but it is good practice
	glBindVertexArray(0);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	// Scale the fonts
	// This will break if windows isn't intsalled on the C drive, TODO: fix that
	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", floor(13 * dpiScale));
	ImGui::StyleColorsDark();
	// Scale everything else
	ImGui::GetStyle().ScaleAllSizes(dpiScale);
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	// GUI state variables
	float size = 1.0f;
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	char buf[2] = { '\0' };
	bool solve = false;
	int currCell = 0;
	bool given[9][9];
	memset(given, 0, sizeof(given));
	vector<vector<char>> sudokuBoard {{'5', '3', '.', '.', '7', '.', '.', '.', '.'},
		{ '6', '.', '.', '1', '9', '5', '.', '.', '.' },
		{ '.', '9', '8', '.', '.', '.', '.', '6', '.' },
		{ '8', '.', '.', '.', '6', '.', '.', '.', '3' },
		{ '4', '.', '.', '8', '.', '3', '.', '.', '1' },
		{ '7', '.', '.', '.', '2', '.', '.', '.', '6' },
		{ '.', '6', '.', '.', '.', '.', '2', '8', '.' },
		{ '.', '.', '.', '4', '1', '9', '.', '.', '5' },
		{ '.', '.', '.', '.', '8', '.', '.', '7', '9' }};


	while (!glfwWindowShouldClose(window))
	{
		glUseProgram(shaderProgram);
		glBindVertexArray(VAO);

		glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		glBindVertexArray(VAO);
		glBindTexture(GL_TEXTURE_2D, sudokuTexture);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		ImGui::Begin("Mt", NULL, ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBackground);
		vector<ImVec2> buttons = generateCellCoordinates(positionVertices, WIDTH, HEIGHT, 13.0f * dpiScale);
		ImGui::SetWindowPos(buttons[81]);
		ImGui::SetWindowSize(ImVec2(buttons[82].x, buttons[82].y));

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		for (int row = 0; row < 9; row++) {
			for (int col = 0; col < 9; col++) {
				ImGui::SetCursorPos(buttons[(row * 9) + col]);
				buf[0] = sudokuBoard[row][col] == '.' ? '\0' : sudokuBoard[row][col];
				buf[1] = '\0';
				if (given[row][col]) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				}
				ImGui::PushID((row * 9) + col);
				if (ImGui::Button(buf, ImVec2(buttons[82].x / 9, buttons[82].y / 9))) {
					currCell = (row * 9) + col;
				}
				ImGui::PopID();
				if (given[row][col]) {
					ImGui::PopStyleColor();
				}
			}
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();

		ImGui::End();

		ImGui::Begin("Settings", NULL); 
		

	
		glUseProgram(shaderProgram);
		glUniform1f(glGetUniformLocation(shaderProgram, "size"), size);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		buf[0] = sudokuBoard[currCell / 9][currCell % 9];
		ImGui::InputText("##buffer", buf, sizeof(buf));
		sudokuBoard[currCell / 9][currCell % 9] = buf[0];
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::Text("Edit current cell");
		ImGui::PopStyleColor();
		std::string temp = "Current cell: " + to_string(currCell + 1);
		const char *test = temp.c_str();
		ImGui::Text(test);
		ImGui::Checkbox("Solve", &solve);
		if (solve) {
			solveSudoku(sudokuBoard);
		}

		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	// Delete all the objects we've created
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(2, VBO);
	glDeleteBuffers(1, &EBO);
	glDeleteProgram(shaderProgram);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

unsigned int initShaders() {
	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	unsigned int ret = glCreateProgram();
	glAttachShader(ret, vertexShader);
	glAttachShader(ret, fragmentShader);
	glLinkProgram(ret);

	// We no longer need the shaders after we have compiled and linked them
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return ret;
}

// Note that if you want to load a PNG the glTexImage2D call is a bit different 
// (CTRL-F "png" in https://learnopengl.com/Getting-started/Textures)
void initTexture(unsigned int &texture) {
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char *data = stbi_load("sudoku.jpg", &width, &height, &nrChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);
	glBindTexture(GL_TEXTURE_2D, 0);
}

ImVec2 vertexToPixel(float vertexX, float vertexY, int width, int height) {
	float retX = (vertexX + 1.0f) * width / 2;
	// The y coordinates for monitors start at the top and go down 
	// (meaning (0, 0) is at the top left of your screen and not the bottom left)
	// which is why we do height - calculation
	float retY = height - ((vertexY + 1.0f) * height / 2);
	return ImVec2(retX, retY);
}

// TODO: make this function work when top left, bottom left, top right, bottom right are in any order
vector<ImVec2> generateCellCoordinates(float *positionVertices, unsigned int width, unsigned int height, float offset) {
	vector<ImVec2> ret;

	ImVec2 topLeft = vertexToPixel(positionVertices[6], positionVertices[7], width, height);
	topLeft.x += offset;
	topLeft.y += offset;
	ImVec2 bottomRight = vertexToPixel(positionVertices[2], positionVertices[3], width, height);
	bottomRight.x -= offset;
	bottomRight.y -= offset;

	// These two numbers should be the exact same since the board is a square
	// (but they may not compare to be equal since they are floats and floats are tricky)
	float xDist = ((bottomRight.x - topLeft.x) / 9);
	float yDist = ((bottomRight.y - topLeft.y) / 9);
	for (int row = 0; row < 9; row++) {
		for (int col = 0; col < 9; col++) {
			ImVec2 pos = ImVec2(xDist * col, yDist * row);
			ret.push_back(pos);
		}
	}

	// We push the top left coordinate and the size so we know where to put the window for ImGui
	ret.push_back(topLeft);
	ret.push_back(ImVec2(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y));

	return ret;
}

bool isCellOkInRow(vector<vector<char>> &board, unsigned int row, unsigned int col, bool allowWildcard = true) {
	if (board[row][col] == '.') {
		if (allowWildcard) {
			return true;
		}

		return false;
	}

	for (int i = 0; i < 9; i++) {
		if (board[row][i] == board[row][col] && i != col) {
			return false;
		}
	}

	return true;
}

bool isCellOkInCol(vector<vector<char>> &board, unsigned int row, unsigned int col, bool allowWildcard = true) {
	if (board[row][col] == '.') {
		if (allowWildcard) {
			return true;
		}

		return false;
	}

	for (int i = 0; i < 9; i++) {
		if (board[i][col] == board[row][col] && i != row) {
			return false;
		}
	}

	return true;
}

bool isCellOkInSubSquare(vector<vector<char>> &board, unsigned int row, unsigned int col, bool allowWildcard = true) {
	if (board[row][col] == '.') {
		if (allowWildcard) {
			return true;
		}

		return false;
	}
	int xOffset = 3 * (col / 3);
	int yOffset = 3 * (row / 3);
	for (int i = 0; i < 9 / 3; i++) {
		for (int j = 0; j < 9 / 3; j++) {
			if (board[i + yOffset][j + xOffset] == board[row][col] && i + yOffset != row && j + xOffset != col) {
				return false;
			}
		}
	}

	return true;
}

bool isCellOk(vector<vector<char>> &board, int row, int col, bool allowWildcard = true) {
	if (!isCellOkInRow(board, row, col, allowWildcard) || !isCellOkInCol(board, row, col, allowWildcard) || !isCellOkInSubSquare(board, row, col, allowWildcard)) {
		return false;
	}

	return true;
}

// This function assumes that every cell before the argument cell is not a wildcard, since
// this function will never be used unless every previous cell has been actualized into a number
bool incrementBoard(vector<vector<char>> &board, int &row, int &col, bool given[9][9]) {
	if (board[row][col] == '.') {
		return false;
	}

	if (board[row][col] - '0' < 9) {
		board[row][col]++;
		return true;
	}

	else {
		board[row][col] = '.';
		if (col > 0) {
			col--;
		}

		else if (row > 0) {
			row--;
			col = 8;
		}

		else {
			return false;
		}

		while (given[row][col]) {
			if (col > 0) {
				col--;
			}

			else if (row > 0) {
				row--;
				col = 8;
			}

			else {
				return false;
			}
		}

		return incrementBoard(board, row, col, given);
	}
}

void solveSudoku(vector<vector<char>> &board) {
	// Set up a bool 2d array that exists to show which squares are given to us and unchangeable
	bool given[9][9];
	memset(given, 0, sizeof(given));
	for (int row = 0; row < 9; row++) {
		for (int col = 0; col < 9; col++) {
			if (board[row][col] != '.') {
				given[row][col] = true;
			}
		}
	}

	for (int row = 0; row < 9; row++) {
		for (int col = 0; col < 9; col++) {
			// If the cell can't be changed then we don't have any work to do on it
			if (given[row][col]) {
				continue;
			}

			if (board[row][col] == '.') {
				board[row][col] = '1';
			}

			while (!isCellOk(board, row, col)) {
				incrementBoard(board, row, col, given);
			}
		}
	}
}