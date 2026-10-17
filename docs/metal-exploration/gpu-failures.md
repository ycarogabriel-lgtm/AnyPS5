# AGC via MoltenVK: evidência e classificação das falhas

Esta investigação verifica a correção do caminho atual Vulkan → MoltenVK → Metal. Não mede FPS nem implementa um renderizador Metal nativo. Os resultados locais aguardam o build e a execução abaixo. Relatos da issue orientam os testes, mas não comprovam o comportamento deste checkout.

## Controles históricos da issue #1

Snapshots em `build-metal/research/`: `comment-68.txt`, `comment-96.txt` e `issue-1-comments-page2.json`. O JSON da página 2 preserva as ressalvas necessárias à interpretação.

| Relato | Ambiente e escopo | Resultado |
|---|---|---|
| [M4](https://github.com/boykopovar/AnyPS5/issues/1#issuecomment-6130236479), comentário 68 | x86-64 sob Rosetta, Apple M4, SDK 1.4.363.0, subgroup 32 | 415 selecionados: 349 passam, 22 falham, 44 skips; todas as falhas reproduzidas |
| [Comparação controlada M2 Max](https://github.com/boykopovar/AnyPS5/issues/1#issuecomment-6174923905), comentário 96 | `macos-port` b7eb6b6b + ImportWatched; 433 selecionados; MoltenVK main 5189f8de e v1.4.2; mesmo SPIR-V, SPIRV-Cross aa217aeb versus 300681bb | 13 falhas antes de SPIRV-Cross #2704, 7 depois; nenhum timeout |

Os denominadores e as revisões diferem. A redução de 22 para 13 não é uma comparação controlada de correções. A redução de 13 para 7 controla a mudança SPIRV-Cross no checkout M2 Max relatado.

SPIRV-Cross [#2704](https://github.com/KhronosGroup/SPIRV-Cross/pull/2704) preserva zero negativo na multiplicação precisa, usando um termo FMA com zero negativo. O relato controlado atribui seis passes à alteração: `dpp16_operands`, `f32_output_modifier`, `float16_fma_rounding`, `float16_rounding`, `packed_alu` e `sdwa_float_selectors`. Em `f32_denormal_flush`, desaparece o erro de sinal do zero, mas permanece outro erro de tininess. O teste experimental separado `mad_f32` cai de 76 divergências para 36, todas de produtos minúsculos. Ele foi adicionado por #5782 e não deve ser contado como teste ordinário daquela suíte.

## Classificação: hipóteses dependem da assinatura local

| Testes ou assinatura | Camada mais sustentada | Correção possível e limite da evidência |
|---|---|---|
| Seis testes sensíveis ao zero negativo acima | Tradução SPIRV-Cross → MSL, demonstrada no controle M2 Max | Driver compilado com #2704, verificando dylib carregada e caches desabilitados. Mesmo nome de teste em outra árvore não prova mesma causa. |
| `f32_denormal_flush`, `f32_constant_factor_tininess`: produto abaixo do mínimo normal arredonda para normal, mas vira zero | Divergência de semântica numérica no caminho Metal; componente exato indefinido | Emulação aritmética no recompiler ou correção do tradutor são possibilidades. Capturar SPIR-V/MSL e reproduzir em Metal antes de chamar de limite inevitável do hardware. `tests/execution/F32ConstantFactorTininess.cpp:147` documenta tininess após arredondamento em RDNA2. |
| `f32_dot2_rounding`: Float64 ausente | Capacidade do dispositivo atual e lowering do recompiler | `SpirvModuleSetup.cpp` rejeita instruções float64 sem Float64. Um lowering por software/inteiros seria trabalho no recompiler, sem habilitar um recurso inexistente do dispositivo. |
| `ngg_vertex_wave_info`: subgroup não suportado no estágio de vértices | Capacidade atual do dispositivo/estágio Vulkan | Lowering por estágio ou emulação exige preservar semântica da wave guest. Substituir tudo por uma lane não é correção equivalente geral. |
| `pixel_interlock`: erro MSL de atributo buffer | Candidato a problema de tradução de interface/descritores em MoltenVK/SPIRV-Cross | O diagnóstico detalhado M2 é erro de compilação; não demonstra impossibilidade geral de interlock no Metal. Preservar MSL completo e configuração de descritores. |
| `relative_ancillary`: capability 2 (Geometry) não suportada | Candidato a declaração inadequada no recompiler | O atual `SpirvBackend/src/SpirvMemory/SpirvModuleSetup.cpp` emite Geometry incondicionalmente para Layer de entrada. O validador já aceita ShaderViewportIndexLayerEXT quando habilitada. A [discussão](https://github.com/boykopovar/AnyPS5/issues/1#issuecomment-6180348495) propõe escolher essa capability suportada. Exige correção e validação dirigida. |
| `dcc_keys_first_write`: imagem pendente inalterada carregada novamente | Candidato a rastreamento de memória host/vida de recurso | Investigar `Execution/src/GuestMemory.cpp::ImportWatched` e as assertivas refresh/version em `tests/execution/DccKeysFirstWrite.cpp`. Essa assertiva de gerenciamento não prova limite de shader Metal. |
| `unchanged_cpu_stamps` | Candidato a write-watch/runtime CPU | Separar proteção de memória/rastreamento no macOS de aritmética GPU. |
| `prepared_shaders_registered_workgroup_sync`: expected preparation failure | Contrato de validação/preparação; indefinido | Conferir o diagnóstico local e a expectativa do teste antes de atribuir ao MoltenVK. |
| Grupo blend M4: alpha-to-coverage, dual-source blend, packed UNORM, sRGB8, swap-alt-blend | Indefinido | Necessários pixels reais/esperados, formatos e recursos habilitados. O nome do teste não distingue recompiler, configuração de função fixa e driver. |
| Grupo imagem M4: array-layer target, depth-compare copy, storage-one-slice alias | Indefinido | Necessários diagnósticos de recursos, views, barreiras e pixels. Não generalizar a assertiva DCC para todo o grupo. |

Os caminhos da tabela partem de `core/libs/prx/libSceAgcDriver/`, exceto o módulo do recompiler, relativo a `core/shader/recompiler/`.

## Ballot e zero negativo exigem investigações separadas

Na página 2, um [experimento em árvore M2 antiga modificada](https://github.com/boykopovar/AnyPS5/issues/1#issuecomment-6178576997) relata um ballot incondicional movido para expressão com curto-circuito, mudando a participação das lanes. Uma transformação restrita para avaliação eager fez aquele kernel passar. Já a [comparação macos-port](https://github.com/boykopovar/AnyPS5/issues/1#issuecomment-6178933798) materializa ballot em fluxo uniforme e passa com #2704 isoladamente. O [retorno](https://github.com/boykopovar/AnyPS5/issues/1#issuecomment-6178982689) aceita essa distinção. Não reescrever todos os ANDs lógicos nem atribuir esse bug ao checkout atual sem inspecionar o módulo gerado.

A discussão também registra que a chave do cache de pipelines Vulkan AGC não identifica a revisão SPIRV-Cross. Um A/B de tradutores deve desabilitar caches e conferir a biblioteca MoltenVK efetivamente carregada. Remover NoContraction é um patch local de desempenho que explicitamente troca precisão, segundo [outro comentário](https://github.com/boykopovar/AnyPS5/issues/1#issuecomment-6179438739); não equivale a uma correção de precisão.

## Auditoria de janelas anterior à execução

A inspeção estática das fontes AGC e do registro CMake encontrou `SDL_CreateWindow` somente em `tests/Visual.cpp:115`, com `SDL_WINDOW_SHOWN`. O alvo `agc_driver_visual_test` é EXCLUDE_FROM_ALL e não tem `add_test`, portanto não entra em `ctest -R '^agc'`. A configuração local também tem `AGC_BUILD_VISUAL_TEST=OFF`. `Flip.cpp` usa um mock de `IVideoOutput`; `HostHdr.cpp` testa estado sem superfície; `WaitShutdown.cpp` testa o encerramento do driver sem VideoOut.

São preservadas exatamente as exclusões históricas: `agc_display_buffer_dcc|agc_driver_flip|agc_driver_lds_atomics64`. A última exclusão independe de janelas. Esta é uma auditoria das fontes, não uma execução de sondagem visual. Não se executam o alvo visual nem jogos.

## Registro local de build e testes

Executado em 17/10/2026 no worktree `Any_PS5-metal` (base `c6dfbc0a3`), Apple M1 16 GiB, macOS 26.6.2, x86-64 sob Rosetta, loader e MoltenVK do Vulkan SDK 1.4.363.0 (`DYLD_LIBRARY_PATH` e `VK_DRIVER_FILES` apontando para o SDK). Seleção `ctest -R '^agc' -E 'agc_display_buffer_dcc|agc_driver_flip|agc_driver_lds_atomics64' -j1 --timeout 30`. Log em `build-metal/ctest-agc.log`, JUnit em `build-metal/agc-results.xml`.

**590 selecionados: 525 passam, 30 falham, 65 skips.** Nenhum timeout.

Recursos anunciados pelo dispositivo (`vulkaninfo`, x86-64 e arm64 iguais): `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`, `descriptorBindingVariableDescriptorCount`, `shaderSampledImageArrayNonUniformIndexing`, `shaderStorageImageArrayNonUniformIndexing`, `shaderClipDistance`, `fragmentShaderPixelInterlock` = true; `geometryShader`, `shaderCullDistance`, `samplerFilterMinmax` = false. Limites comuns por estágio: 256 imagens amostradas, 16 samplers, 287 recursos; limites update-after-bind: 1000000 imagens, 1024 samplers, 1000000 recursos.

| Grupo (causa local) | Testes | Camada |
|---|---|---|
| Capability 2 (`Geometry`) para ler o layer | `layered_volume_draw`, `layered_volume_vertex`, `layered_volume_vertex_no_gpl`, `pixel_ancillary_word`, `relative_ancillary`, `sample_mask_multisample` | Recompilador: declara `Geometry`, que o Metal não tem; há alternativa (`ShaderViewportIndexLayerEXT`/`ShaderLayer`) |
| Zero negativo vira positivo | `dpp16_operands`, `f32_output_modifier`, `float16_fma_rounding`, `float16_rounding`, `packed_alu`, `sdwa_float_selectors` | SPIRV-Cross antes do #2704 dentro do MoltenVK 1.4.2 |
| Tininess/denormal | `f32_constant_factor_tininess`, `f32_denormal_flush` | Semântica numérica Metal; o segundo também tem a parte de sinal |
| "Dispositivo não tem" tabela de imagens/samplers | `bindless_images`, `image_sint8`, `image_table`, `image_table_cache_bounds`, `image_table_reuse` | Driver: a capacidade é medida pelos limites comuns (256), que as reservas fixas de 1792 imagens esgotam; o modo update-after-bind tem 1000000 e não é usado para as tabelas |
| Cull distance | `vertex_clip_cull_distances`, `vertex_clip_cull_draw`, `vertex_clip_cull_draw_no_key`, `vertex_user_data_tail` | Limite real: `shaderCullDistance = false` |
| Redução min/max em sampler | `image_plane_bias_on_single_level_volume` | Limite real: `samplerFilterMinmax = false` |
| Compilação MSL | `pixel_interlock` (`raster_order_group` em campo de argument buffer), `front_face_all_bits` (render pipeline) | MoltenVK/SPIRV-Cross |
| Write watch / memória do host | `write_tracking`, `multisample_guest_access`, `gpu_indirect_arguments_misaligned_writer_ahead` | Runtime macOS (proteção de páginas importadas, escrita GPU em página com várias vistas) |
| Tessellation | `tessellation` (canal azul do índice de patch entre instâncias) | Indefinido; MoltenVK emula tessellation com compute |

## Depois das correções desta branch

Mesma máquina e seleção, após `1c772b1b4` (layer sem `Geometry`) e `6fb4e2f18` + seguinte (tabelas por limites update-after-bind). Log em `build-metal/ctest-agc-after.log`.

- Corrigidos (10): `bindless_images`, `image_sint8`, `image_table`, `image_table_cache_bounds`, `image_table_reuse`, `layered_volume_draw`, `layered_volume_vertex`, `layered_volume_vertex_no_gpl`, `pixel_ancillary_word`, `relative_ancillary`. `sample_mask_multisample` passa da criação do pipeline e falha na máscara de amostras por amostra.
- `graphics` falhou na rodada completa por um contexto de teste que só preenche os limites update-after-bind de storage buffers; corrigido no commit seguinte (o set update-after-bind usa o maior dos dois limites) e passa.
- `stream_copy`: 17 a 29 s contra o limite de 30 s; passa isolado em 5 de 5, estourou o tempo uma vez na rodada completa.
- `store_over_untracked` ("the result did not reach a block the CPU left alone"): passou na rodada de 17/10 00:30, mas falha hoje 3 de 3 também com as fontes anteriores a estas correções e no build `macos/integration`; não é regressão desta branch. Depende de estado da máquina ainda não identificado (não muda com `ANYPS5_NO_SHADER_CACHE=1` nem `APS5_NO_WRITE_WATCH=1`).

Resultado esperado com as correções: 20 falhas das 30 originais, das quais 5 são limites reais do Metal/MoltenVK (cull distance, redução min/max) e 6 dependem de um MoltenVK com SPIRV-Cross #2704.

## MoltenVK com SPIRV-Cross #2704

MoltenVK `main` em `5189f8de` ainda fixa SPIRV-Cross `aa217aeb`, anterior à correção. Build local com a revisão trocada para `300681bb` (merge de #2704):

```sh
git clone https://github.com/KhronosGroup/MoltenVK.git && cd MoltenVK
echo 300681bbcb8365a8d1eacdacdbdd0249bbf1d6b1 > ExternalRevisions/SPIRV-Cross_repo_revision
./fetchDependencies --macos && make macos
```

O resultado (`Package/Release/MoltenVK/dynamic/dylib/macOS`, x86_64 + arm64, MoltenVK 1.4.3) só é usado se a pasta dele vier **antes** da pasta do SDK em `DYLD_LIBRARY_PATH`: o manifesto aponta para `./libMoltenVK.dylib` e o dyld procura esse nome primeiro no `DYLD_LIBRARY_PATH`, onde o SDK tem outra cópia. Conferir pelo MSL (`MVK_CONFIG_SHADER_DUMP_DIR`): o helper deve gerar `fma(l, r, T(-0.0))`.

Na branch `macos/integration` (`bbd10b011`), com esse MoltenVK: **590 selecionados, 510 passam, 15 falham, 65 skips** (eram 30 falhas no começo desta exploração). Os seis testes de zero negativo passam; `f32_denormal_flush` e `f32_constant_factor_tininess` seguem falhando por tininess. Nova instabilidade: `cross_workgroup_flag` passou 1 de 3 isolado com este driver e passou nas duas rodadas com o MoltenVK do SDK.

## Primeiro título

DOOM Shareware (homebrew PPSA99666 v1.1.1, `lowbit/ps5-doom`, SHA-256 conferido), `tools/unwrap_self.py` no `eboot.bin` e no `sce_module/libc.prx`, `relinker --macos --to-rosetta`, bibliotecas de `macos/integration` em `2426107cf`, este MoltenVK: passa do launcher, carrega `doom1.wad` e desenha o menu de episódios sobre a demo (quadros salvos com `APS5_DUMP_FRAMES`). A correção necessária foi `sceKernelInternalMemoryGetModuleSegmentInfo` no macOS.
