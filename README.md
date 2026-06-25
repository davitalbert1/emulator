# Antigravity Multi-Console Emulator

Um emulador multi-consoles moderno escrito em C++ com suporte à simulação de arquitetura de CPU, vídeo e áudio para **13 consoles clássicos e modernos**. O projeto utiliza **CMake** para compilação, **SDL2** para gerenciar a camada de vídeo/áudio e **Dear ImGui** para fornecer um painel de depuração robusto e fixo (dashboard).

---

## 🎮 Consoles Suportados

O emulador organiza os sistemas em categorias portáteis e de mesa:

| Console | Tipo | Resolução Nativa | CPU(s) Simulada(s) | Extensões de ROM |
| :--- | :---: | :---: | :--- | :--- |
| **Game Boy Advance (GBA)** | Portátil | 240x160 | ARM7TDMI (32-bit) | `.gba` |
| **Game Boy Clássico (GB)** | Portátil | 160x144 | Sharp LR35902 (8-bit) | `.gb` |
| **Game Boy Color (GBC)** | Portátil | 160x144 | Sharp LR35902 (8-bit) | `.gbc` |
| **Nintendo DS (NDS)** | Portátil | 256x392 *(Dupla)* | ARM946E-S + ARM7TDMI | `.nds` |
| **Nintendo 2DS** | Portátil | 400x488 *(Dupla)* | ARM11 Dual + ARM9 | `.3ds`, `.cia` |
| **Nintendo 3DS** | Portátil | 400x488 *(Dupla)* | ARM11 Dual + ARM9 | `.3ds`, `.cia` |
| **Atari 2600** | Mesa | 160x192 | MOS 6507 (8-bit) | `.a26`, `.bin` |
| **Nintendo (NES)** | Mesa | 256x240 | Ricoh 2A03 (8-bit) | `.nes` |
| **Super Nintendo (SNES)** | Mesa | 256x224 | Ricoh 5A22 (16-bit) | `.sfc`, `.smc` |
| **Sega Genesis / Mega Drive** | Mesa | 320x224 | Motorola 68000 (16/32-bit) | `.gen`, `.md` |
| **Nintendo 64 (N64)** | Mesa | 320x240 | MIPS VR4300 (64-bit) | `.z64`, `.n64`, `.v64` |
| **Nintendo Wii** | Mesa | 640x480 | IBM Broadway (PowerPC) | `.wbfs`, `.iso`, `.gcm` |
| **Nintendo Switch** | Mesa | 1280x720 | ARMv8-A Cortex-A57 | `.nsp`, `.xci`, `.nro` |

*Nota sobre telas duplas:* O DS, 2DS e 3DS possuem displays empilhados verticalmente com um espaçador escuro de 8 pixels, perfeitamente integrados ao loop gráfico da aplicação.

---

## 🚀 Como Compilar (Windows)

Esta aplicação foi configurada para compilação estática com **MinGW GCC**, garantindo portabilidade total (o executável roda em qualquer Windows sem a necessidade de DLLs externas).

### Pré-requisitos
Instale o [MSYS2](https://www.msys2.org/) e configure o ambiente com o GCC, CMake e Ninja. No terminal do MSYS2 MINGW64, execute:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja git
```

### Instruções de Compilação (PowerShell / CMD)
Abra o terminal do Windows na raiz do projeto e execute os seguintes comandos:

```powershell
# 1. Adicionar o compilador e ferramentas do MSYS2 ao PATH temporariamente
$env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:PATH

# 2. Configurar o diretório de build com o Ninja
cmake -B build_win -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# 3. Compilar o executável
cmake --build build_win --config Release
```

O executável portátil será gerado em: `build_win/emulator.exe`.

---

## 🛠️ Recursos de Interface & Engenharia

1. **Inicializador Inteligente (Launcher):**
   Um menu inicial em duas colunas abre antes do emulador principal. Permite selecionar facilmente o hardware desejado. O menu superior "Consoles" ou o atalho `Ctrl+N` permite retornar ao launcher.
2. **Dashboard Anti-Overlap Fixado:**
   As janelas do ImGui são fixas nas suas posições ideais (`ImGuiCond_Always` e flags `NoMove \| NoResize \| NoCollapse`). A tela é dividida logicamente em display LCD, CPU control, registradores e leitor de cabeçalho.
3. **Diálogo de Arquivos Nativo do Windows (Modal):**
   Utiliza a API nativa `GetOpenFileNameA` integrada diretamente com a janela de propriedade do SDL2 (`HWND`), garantindo foco modal robusto e filtros específicos de extensão para as ROMs do console ativo.
4. **Wow Factor Visuals:**
   Os modos de teste geram gráficos dinâmicos temáticos para os consoles (ex: fundo azul clássico e Mario pulando no NES, efeito tridimensional **Mode 7** no SNES, e loops e colinas em paralaxe de *Green Hill Zone* com o Sonic no Genesis).

---

## 📁 Estrutura de Diretórios do Projeto

- `include/`: Declaração das classes de console, contratos da base `Console` e gerenciadores da aplicação.
- `src/`: Implementações em C++ para CPUs, geradores gráficos, parser de cabeçalhos de ROMs e loop de renderização da aplicação.
- `vendor/`: Dependências locais de build estático do SDL2 e Dear ImGui gerenciadas via CMake `FetchContent` (baixadas automaticamente no primeiro build).
- `CMakeLists.txt`: Script central do CMake configurado para linkagem estática.
