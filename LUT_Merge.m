%% Merge irradiance and temperature sweeps into 2-D Pmpp table
Sg = load('pmpp_25C_from_plot.mat');     % has G_vec, Pmpp_vals
St = load('pmpp_1000_vs_T.mat');         % has T_grid, Pmpp_1000_vs_T

G_grid         = Sg.G_vec(:).';          % 1 x NG  (row)
Pmpp_G_at_25   = Sg.Pmpp_vals(:).';      % 1 x NG  (row)

T_grid         = St.T_grid(:).';         % 1 x NT  (row)
Pmpp_1000_vs_T = St.Pmpp_1000_vs_T(:).'; % 1 x NT  (row)

% Anchor at 25°C (or nearest)
[~, i25] = min(abs(T_grid - 25));
scale_T  = (Pmpp_1000_vs_T / Pmpp_1000_vs_T(i25));   % 1 x NT (row)

% ---- FIX: make it column x row for outer product ----
Pmpp_table = scale_T(:) * Pmpp_G_at_25(:).';   % (NT x NG)

save pmpp_G_T_table.mat G_grid T_grid Pmpp_table

% Plot check
figure;
surf(G_grid, T_grid, Pmpp_table);
xlabel('Irradiance G (W/m^2)');
ylabel('Temperature (°C)');
zlabel('P_{MPP} (W)');
title('Oracle P_{MPP}(G,T) surface');
