%% Minimal Pmpp extractor (tolerant & order-agnostic)
fig = gcf;
axs = findall(fig,'Type','axes');

% Find the P–V axes by Y-label containing 'Power'
ylabs = arrayfun(@(a) string(get(get(a,'YLabel'),'String')), axs, 'UniformOutput', true);
idxPV = find(contains(ylabs, "Power", 'IgnoreCase', true), 1);
assert(~isempty(idxPV), 'Could not find P–V axes.');
axPV = axs(idxPV);

% Pull all line objects from the P–V axes
lns = findobj(axPV,'Type','line');
assert(~isempty(lns), 'No line objects found on the P–V axes.');

% Get the max power from each line
Pmax_all = arrayfun(@(h) max(get(h,'YData')), lns);

% Collapse duplicates
Pmpp_vals = uniquetol(Pmax_all, 1e-9);

% Sort ascending
Pmpp_vals = sort(Pmpp_vals, 'ascend');

% Irradiance vector
G_vec = [200 300 400 500 600 700 800 900 1000];

% If counts mismatch, warn but still save (you can re-run with matching G_vec)
if numel(Pmpp_vals) ~= numel(G_vec)
    warning('Count mismatch: found %d unique curves, but G_vec has %d entries.', ...
            numel(Pmpp_vals), numel(G_vec));
    % If needed, truncate or pad
    n = min(numel(Pmpp_vals), numel(G_vec));
    Pmpp_vals = Pmpp_vals(1:n);
    G_vec = G_vec(1:n);
end

save pmpp_25C_from_plot.mat G_vec Pmpp_vals

% Quick plot
figure; plot(G_vec, Pmpp_vals, 'o-'); grid on
xlabel('Irradiance G (W/m^2)'); ylabel('P_{MPP} (W)')
title('Extracted P_{MPP}(G) from PV Array plot')
