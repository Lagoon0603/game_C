# コンパイラの指定
CC = clang

# ソースファイルと出力ファイル名
SRC = main.c
TARGET = game

# Homebrewでインストールされたraylibのパスを自動取得
# (M1/M2/M3 Macの /opt/homebrew にも Intel Macの /usr/local にも対応)
RAYLIB_PATH = $(shell brew --prefix raylib)

# コンパイルフラグ
# -Wall: 警告を全て出す（バグ発見に役立つ）
# -std=c99: C99規格を使用
# -I: ヘッダーファイルの場所を指定
CFLAGS = -Wall -std=c99 -I$(RAYLIB_PATH)/include

# リンカフラグ
# -L: ライブラリファイルの場所を指定
# -lraylib: raylibをリンク
# -framework ...: macOS特有の必須フレームワーク
LDFLAGS = -L$(RAYLIB_PATH)/lib -lraylib \
          -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL

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