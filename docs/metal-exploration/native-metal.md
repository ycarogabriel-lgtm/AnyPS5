# Backend Metal nativo: exploração estática

## Parecer

**Há implementação substancial reutilizável, mas não é uma troca de biblioteca Vulkan.** O fork mantém execução PM4, memória guest, recompilação e apresentação próprias para Metal. Recomendo primeiro um replay isolado com contrato de memória explícito; só depois integrar ao runtime local. A existência do código e de testes não comprova compatibilidade de jogos nem desempenho nesta máquina.

Escopo: leitura de fontes de RINNECODER/AnyPS5 no SHA `c48d292e1913cbfd6fccd50c9bb371ce07d2c3db`, baixadas com curl em `build-metal/research/rinne`. Nenhum build, teste GPU, janela ou execução de jogo foi realizado nesta avaliação. Estimativas abaixo são julgamento de engenharia, não medição.

## O que foi verificado

| Camada | Evidência estática e consequência |
|---|---|
| Entrada e execução | `Execution/Metal/MetalDriver.mm`, `MetalPacketExecution.mm` e `MetalDrawAhead.mm` implementam o caminho Metal em torno do estado/packets guest. Não basta copiar `Graphics/Metal`. |
| Shaders | `MetalComputeDispatch.mm:177–189` chama `Recompile` e `ConvertToMetal`; `MetalDraw.mm:1394` converte o vertex/mesh e também converte fragment/tessellation. O caminho é RDNA guest → frontend/IR e SPIR-V existentes → SPIRV-Cross customizado → MSL → pipeline Metal. Não é tradutor RDNA→MSL independente. |
| Bridge | `MetalShaderBridge.cpp` deriva `CompilerMSL`, adapta instruções/recursos e valida `runtimeAbiVersion`, metadata de bindings, slots reservados e interfaces. Requer os headers/ABI e alterações compatíveis do recompilador do fork. |
| Recursos e memória | `MetalShaderResources.mm` valida `RuntimeAbi`; `MetalGuestMemory.mm` usa buffers shared, GPU addresses, mapeamentos Mach e limites de buffers. Texturas, detile, depth, samplers, cópias de retorno e sincronização têm implementações específicas. |
| Apresentação | `NativeMetalSession.mm` exige thread principal AppKit, ownership das faixas guest e metallib existente; faz shutdown/drain do driver e janela. `RosettaGraphics.cmake` também integra VideoOut e host window. |
| Build completo | `cmake/RosettaGraphics.cmake` monta explicitamente driver, utilitários, bridge, frontend, PRXs e VideoOut. `ANYPS5_METAL_UTILITIES_ONLY` retorna antes do runtime completo; sucesso desse alvo não validaria jogo. |
| Cache e suporte | Há cache de conversão MSL em disco, cache de pipelines e shaders utilitários `.metal`. O conjunto Graphics/Metal + MetalBackend tem cerca de 17 mil linhas; o número não inclui Execution/Metal, VideoOut, frontend e build. |

Fontes pinned: [integração do runtime](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/cmake/RosettaGraphics.cmake), [compute](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Graphics/Metal/MetalComputeDispatch.mm#L177), [draw](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Graphics/Metal/MetalDraw.mm#L1394), [bridge](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/shader/recompiler/MetalBackend/MetalShaderBridge.cpp#L1101), [recursos](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Graphics/Metal/MetalShaderResources.mm#L139), [sessão](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceVideoOut/src/NativeMetalSession.mm#L63).

## QEMU, arm64 e Rosetta

**Neste SHA o caminho publicado é Rosetta direto x86_64.** O README pede Apple Silicon + Rosetta 2, `ANYPS5_ROSETTA_LIBS=ON` e `CMAKE_OSX_ARCHITECTURES=x86_64`. Isso não demonstra um runtime nativo arm64 completo.

`DirectDriver.hpp:8–10` distingue expressamente as faixas identidade da execução direta dos mapeamentos traduzidos do engine TCG. O driver recebe faixas emprestadas com ownership e permissões; sua interface pode servir a outro executor, mas plugar um executor QEMU exige adaptar endereço guest→host, vida das alocações, coerência CPU/GPU, eventos EOP e submissão. Não encontrei chamadas QEMU nas fontes Graphics/Metal e Execution/Metal examinadas. Isso indica separação por contrato, não ausência de dependências no restante do projeto.

Há risco real de memória documentado no próprio código: páginas file-backed aliased sem cópia causavam SIGBUS na execução Rosetta; `MetalGuestMemory.mm:46–49` escolhe cópia para esses casos. Copiar o driver sem esse tratamento pode reintroduzir a falha. Portar apenas o bridge para arm64 não resolve ABI x86_64, relinker, syscalls ou execução CPU.

Fontes: [README](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/README.md#L3), [contrato DirectDriver](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Execution/Metal/DirectDriver.hpp#L8), [memória](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Graphics/Metal/MetalGuestMemory.mm#L46).

## Dependências e limites

- Apple: Objective-C++20 com ARC, Xcode/`xcrun metal` e `metallib`, Foundation, Metal, QuartzCore; runtime também AppKit/Carbon, CoreGraphics/ImageIO. MetalFX é weak-linked. Não há mínimo macOS fixado no README, portanto precisa definir e testar uma matriz de SDK/OS/GPU.
- SPIRV-Cross é submodule pinned em `aa217aeb6c9f0ace7a0ab233b28807edf45eb165`; o bridge liga `spirv-cross-msl` e `compression`. `RosettaGraphics.cmake` ainda exige SPIRV-Headers, Vulkan-Headers e glslang. Usar headers Vulkan não significa executar Vulkan/MoltenVK.
- O bridge exige MSL ≥2.2 e mesh MSL ≥3.0. Rejeita atomics Int64 no backend pinned; tessellation genérica sem contrato de scheduling; alguns contratos implícitos/atomics de imagem; bindings que ultrapassam slots. `MetalDraw.mm` ainda rejeita resolve de color target inteiro, alguns indexed fast-launch e triangle fan geometry com primitive restart. Há emulações específicas, não paridade total de RDNA/Vulkan.
- Testes/replays estão registrados para utilitários, shader bridge, PM4, texturas, apresentação e semântica. Foram inspecionados nomes/build, sem executar; não são evidência de aprovação nesta exploração.
- README declara GPL-2.0-only. Uma incorporação deve preservar licença, atribuição e proveniência por commit.

Fontes: [build utilitários](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Graphics/Metal/CMakeLists.txt), [build bridge](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/shader/recompiler/MetalBackend/CMakeLists.txt), [submodule pinned](https://github.com/KhronosGroup/SPIRV-Cross/tree/aa217aeb6c9f0ace7a0ab233b28807edf45eb165), [limites bridge](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/shader/recompiler/MetalBackend/MetalShaderBridge.cpp#L1125), [limites draw](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Graphics/Metal/MetalDraw.mm#L646).

## Quais falhas do caminho MoltenVK o backend nativo resolve?

A matriz separa implementação observada de resultado GPU: **nenhuma linha foi validada executando shaders nesta exploração**. “Potencial” não significa correção demonstrada.

| Falha | Backend Metal nativo do SHA analisado | Evidência e limite |
|---|---|---|
| Float64 | **Não resolve** | O bridge rejeita `CapabilityFloat64` com “requires explicit software emulation” (`MetalShaderBridge.cpp:1223`). Ser nativo Metal não fornece emulação f64. |
| Int64 atomics | **Não resolve** | Rejeição explícita em `:1224`, atribuída ao SPIRV-Cross pinned. Inteiros 64-bit comuns são capability separada e não comprovam atomics. |
| Signed-zero, SPIRV-Cross #2704 | **O pin ainda contém a causa do bug** | O `spirv_msl.cpp` aa217, linha 6450, gera `fma(l, r, T(0))`. `InlineExactArithmetic` (`bridge:28–57`, chamada em `:1569`) só remove `optnone` e desabilita contração; não substitui o addend +0 por -0. Assim o caminho relevante continua vulnerável por inspeção estática. Atualizar/cherry-pick #2704 e repetir fixture é necessário; não afirmamos falha GPU reproduzida aqui. |
| Subgroup em vertex | **Contorno específico implementado** | `DefineVertexSubgroupLane` (`:550–585`) troca o builtin recusado por SPIRV-Cross por lane calculada via `simd_prefix_exclusive_sum(1u)` na entrada. `:1553–1562` fixa subgroup de 32 e rejeita outro contrato explícito. Pode remover esse bloqueio do tradutor Vulkan→Metal, mas não prova qualquer wave64/divergência correta. |
| Pixel interlock | **Potencial, sem prova no caminho guest** | SPIRV-Cross aa217 reconhece Begin/EndInvocationInterlock (`:10744`) e associa recursos a `raster_order_group(0)` (`:15636` e seguintes), exigindo MSL ≥2.0. Não localizei implementação/validação específica de pixel interlock no bridge. É necessário comprovar que frontend emite o interlock, recursos recebem a decoração e pipeline/Apple GPU preservam ordering; evitar o gate Vulkan pode bastar para disponibilidade, não para correção. |
| Tininess, denormals e arredondamento | **Não há correção geral demonstrada** | `MetalPipelineCache.mm:578–582` seleciona fast/safe math; pragma anti-contração não define sozinho tininess-before/after-rounding nem semântica RDNA de flush. Não encontrei tratamento nomeado de tininess no bridge/utilitários examinados. Exige fixture nos limites normal/subnormal e modos FLOAT_MODE; ausência de palavra não prova ausência de tratamento no frontend. |
| DCC e writewatch/coerência | **Tratamento dedicado parcial, não solução automática** | `MetalShaderResources.mm` calcula chaves DCC, valida escritas e `markDccUncompressed` (`:1018`) atualiza metadata; invalidação observa overlaps (`:1093`). `MetalGuestMemory` usa faixas emprestadas e cópias. Isso pode contornar partes do caminho Vulkan, mas depende de callbacks de stores/CPU-read e coerência no executor; não foi verificado um writewatch completo nem toda codificação DCC. |
| Loader/WSI Vulkan no macOS | **Evita essa classe de falha** | Usa AppKit/CAMetalLayer e pipeline Metal próprio. Mantém exigências de thread principal, metallib e integração VideoOut. |
| Compilação C++/chrono, GuestAllocations, ABI/PRX e execução CPU | **Não resolve por si só** | Backend gráfico não corrige runtime/relinker ou ABI. |

Fontes pinned: [bridge e Float64/atomics](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/shader/recompiler/MetalBackend/MetalShaderBridge.cpp#L1221), [helpers de aritmética](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/shader/recompiler/MetalBackend/MetalShaderBridge.cpp#L28), [helper upstream ainda com +0](https://github.com/KhronosGroup/SPIRV-Cross/blob/aa217aeb6c9f0ace7a0ab233b28807edf45eb165/spirv_msl.cpp#L6445), [contorno vertex](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/shader/recompiler/MetalBackend/MetalShaderBridge.cpp#L550), [interlock no pin](https://github.com/KhronosGroup/SPIRV-Cross/blob/aa217aeb6c9f0ace7a0ab233b28807edf45eb165/spirv_msl.cpp#L10744), [math options](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Graphics/Metal/MetalPipelineCache.mm#L578), [DCC](https://github.com/RINNECODER/AnyPS5/blob/c48d292e1913cbfd6fccd50c9bb371ce07d2c3db/core/libs/prx/libSceAgcDriver/Graphics/Metal/MetalShaderResources.mm#L1018).

[#2704](https://github.com/KhronosGroup/SPIRV-Cross/pull/2704) foi merged em 14/10/2026 e troca o addend/accumulators por -0 para preservar o sinal; isso não atualiza automaticamente o submodule aa217 do fork. O [README MoltenVK consultado](https://github.com/KhronosGroup/MoltenVK/blob/5189f8de86ec365860f63bf1da736815cfaea5cd/README.md) anuncia depreciação pela Khronos e manutenção comunitária, recomendando KosmicKrisp para Apple Silicon. É contexto de manutenção, sem avaliação aqui de compatibilidade ou substituição por KosmicKrisp. MoltenVK traduz um subconjunto Vulkan/SPIR-V sobre Metal; não conhece semântica PS5/DCC nem corrige frontend guest automaticamente.

## Estimativa de incorporação

Pessoa-semanas de trabalho efetivo, para engenheiro sênior com C++/Objective-C++, shaders e depuração GPU; faixas incluem diagnóstico e revisão. Não equivalem a prazo de calendário, nem promessa de jogos compatíveis.

| Entrega e critério de saída | Estimativa |
|---|---:|
| Congelar revisão, mapear divergências de frontend/ABI/build e dependências | 1–2 |
| Portar bridge e utilitários com compilação e contratos CPU/replays mínimos | 2–4 |
| Integrar driver PM4, memória, recursos, eventos e apresentação em um executor escolhido | 4–8 |
| Corrigir semântica com fixtures/replays representativos e regressões Vulkan | 3–6 |
| Validar pequeno conjunto acordado de títulos, empacotar e medir desempenho | 2–6 |
| **Total de primeira integração delimitada** | **12–26** |

Hipótese de planejamento: aproveitar o frontend compatível do fork e selecionar **um** caminho CPU (Rosetta direto ou executor com adaptador). Reescrever scheduling ausente, expandir cobertura de jogos ou construir execução arm64 completa fica fora desse total e pode superar a integração inicial. Uma prova isolada bridge+compute é plausível em **2–4 pessoa-semanas**, mas não comprova viabilidade de um título inteiro.

Próximo gate: comparar `RecompileResult`, `RuntimeAbi`, `BdaAbi`, PM4 e NativeGuestMemory entre os SHAs; produzir uma lista de alterações mínimas antes de cherry-pick. Depois validar um replay compute determinístico e um draw offscreen, e só então VideoOut/janela e jogo. Nesta exploração esses gates permanecem pendentes.
