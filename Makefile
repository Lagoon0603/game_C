# コンパイラの指定
CC = clang

# ソースファイルと出力ファイル名
SRC = main.c
TARGET = game

# OS判定
UNAME_S := $(shell uname -s)

# コンパイルフラグ
# -Wall: 警告を全て出す（バグ発見に役立つ）
# -std=c99: C99規格を使用
CFLAGS = -Wall -std=c99

# リンカフラグ
LDFLAGS =

ifeq ($(UNAME_S),Linux)
    # Linux用の設定
    # raylibが標準パスにあると仮定
    LDFLAGS += -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
else
    # macOS用の設定
    # Homebrewでインストールされたraylibのパスを自動取得
    # (M1/M2/M3 Macの /opt/homebrew にも Intel Macの /usr/local にも対応)
    RAYLIB_PATH = $(shell brew --prefix raylib)
    CFLAGS += -I$(RAYLIB_PATH)/include
    LDFLAGS += -L$(RAYLIB_PATH)/lib -lraylib \
              -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
endif

# --- コマンド定義 ---

# 「make」とだけ打った時に実行されるデフォルトターゲット
all: $(TARGET)

# コンパイルのルール
$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

# コンパイルしてすぐに実行するコマンド「make run」
run: all
	./$(TARGET)

# 生成ファイルを削除するコマンド「make clean」
clean:
	rm -f $(TARGET)