# src/tools

Modulos auxiliares reaproveitaveis do core.

Objetivo:
- reduzir tamanho e acoplamento de `kernel.cpp`;
- concentrar blocos mutaveis (USB-HID, mapeadores de input, helpers de runtime)
  em componentes menores, com interface clara.

Regra pratica:
- `kernel.cpp` fica como orquestrador;
- comportamento especifico de subsistemas vai para `src/tools/<modulo>/*.h` + `*.cpp`.

Modulos atuais:
- `input/`
  - `keyboard_input.*`: decodificacao de teclado para a camada de menu/atalhos.
  - `usb_hid_keyboard.*`: ciclo USB HID (attach/detach, callback raw, snapshot lockless).
- `locale/`
  - `combo_locale.*` e tabelas `combo_locale_{pt,en,es}.cpp`: i18n/locales.
- `debug/`
  - `combo_debug_overlay.*`: overlay de debug HDMI.
- `menu/`
  - `combo_menu.*` e `combo_menu_items.*`: menu de pausa e definicao de itens.
- `file_browser/`
  - `combo_file_browser.*`: navegador de arquivos para ROM/Disk.
- `render/`
  - `gfx_textbox.*`: renderer de UI/texto.
  - `viewport_screenshot.*`: captura de viewport para fundo do menu e save-state UX.
- `media/`
  - `media_catalog.*`: descoberta/listagem de midias.
